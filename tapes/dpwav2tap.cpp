// dpwav2tap-integrated-v11-score.cpp
// Version: multi-region checksum recovery + pattern tie-break v4.1, 2026-08-31
//
// Datapoint 2200 cassette FSK WAV decoder.
//
// This program is derived from the time-domain decoder in dpwav2tap.cpp,
// and integrates Datapoint block framing, validation and SIMH TAP output.
// Optional framed bit and raw timing outputs are available for diagnostics.
//
// Output format:
//   '0' and '1' are confidently decoded bits.
//   '?' marks a transition which the timing decoder considered abnormal.
//   Newlines are inserted only for readability and have no timing meaning.
//
// A CSV trace can optionally be generated.  It records the audio sample
// position, decoded bit and current PLL period, which is useful when a bit
// needs to be compared with the waveform and edited by hand.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using sample_t = int16_t;

static int verbosity = 0;

static void vprint(int level, const char *fmt, ...) {
    if (level > verbosity) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

[[noreturn]] static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    std::exit(1);
}

static uint16_t read_u16_le(FILE *fp) {
    uint8_t b[2];
    if (fread(b, 1, sizeof(b), fp) != sizeof(b)) {
        die("unexpected end of WAV file");
    }
    return static_cast<uint16_t>(b[0]) |
           (static_cast<uint16_t>(b[1]) << 8);
}

static uint32_t read_u32_le(FILE *fp) {
    uint8_t b[4];
    if (fread(b, 1, sizeof(b), fp) != sizeof(b)) {
        die("unexpected end of WAV file");
    }
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

struct WavData {
    uint32_t sample_rate = 0;
    std::vector<sample_t> samples;
};

static WavData read_wav(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        std::exit(1);
    }

    char id[4];
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "RIFF", 4) != 0) {
        die("input is not a RIFF file");
    }
    (void)read_u32_le(fp); // RIFF size; not needed for decoding.
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "WAVE", 4) != 0) {
        die("input is not a WAVE file");
    }

    bool have_fmt = false;
    bool have_data = false;
    uint16_t format = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t sample_rate = 0;
    std::vector<uint8_t> raw_data;

    // WAV files can contain ancillary chunks.  Unlike the old program,
    // search for the fmt and data chunks rather than assuming adjacency.
    while (!have_data) {
        if (fread(id, 1, 4, fp) != 4) {
            break;
        }
        uint32_t chunk_size = read_u32_le(fp);
        long chunk_start = ftell(fp);

        if (memcmp(id, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                die("WAV fmt chunk is too short");
            }
            format = read_u16_le(fp);
            channels = read_u16_le(fp);
            sample_rate = read_u32_le(fp);
            (void)read_u32_le(fp); // byte rate
            (void)read_u16_le(fp); // block align
            bits_per_sample = read_u16_le(fp);
            have_fmt = true;
        } else if (memcmp(id, "data", 4) == 0) {
            raw_data.resize(chunk_size);
            if (chunk_size != 0 && fread(raw_data.data(), 1, chunk_size, fp) != chunk_size) {
                die("truncated WAV data chunk");
            }
            have_data = true;
        }

        if (!have_data) {
            // Chunks are padded to an even byte boundary.
            long next = chunk_start + static_cast<long>(chunk_size) + (chunk_size & 1U);
            if (fseek(fp, next, SEEK_SET) != 0) {
                die("cannot seek over WAV chunk");
            }
        }
    }

    fclose(fp);

    if (!have_fmt || !have_data) {
        die("WAV file lacks fmt or data chunk");
    }
    if (format != 1) {
        die("only uncompressed PCM WAV input is supported");
    }
    if (channels != 1 && channels != 2) {
        die("only mono or stereo WAV input is supported");
    }
    if (bits_per_sample != 8 && bits_per_sample != 16) {
        die("only 8-bit or 16-bit PCM WAV input is supported");
    }

    const size_t bytes_per_sample = bits_per_sample / 8;
    const size_t frame_bytes = bytes_per_sample * channels;
    if (frame_bytes == 0 || raw_data.size() % frame_bytes != 0) {
        die("invalid WAV data length");
    }

    WavData wav;
    wav.sample_rate = sample_rate;
    const size_t frames = raw_data.size() / frame_bytes;
    wav.samples.reserve(frames);

    size_t p = 0;
    for (size_t i = 0; i < frames; ++i) {
        int32_t sum = 0;
        for (uint16_t ch = 0; ch < channels; ++ch) {
            int32_t s;
            if (bits_per_sample == 8) {
                // 8-bit PCM WAV samples are unsigned.
                s = (static_cast<int32_t>(raw_data[p++]) - 128) << 8;
            } else {
                uint16_t u = static_cast<uint16_t>(raw_data[p]) |
                             (static_cast<uint16_t>(raw_data[p + 1]) << 8);
                p += 2;
                s = static_cast<int16_t>(u);
            }
            sum += s;
        }
        wav.samples.push_back(static_cast<sample_t>(sum / channels));
    }

    return wav;
}


// Remove slow DC/baseline wander before peak detection.  Datapoint carrier
// transitions are around 0.5-1.0 kHz at this tape speed, while cassette
// head/coupling/tape defects can add much slower baseline motion.  A symmetric
// configurable moving average follows that slow component.  Subtracting it
// makes local extrema and knee timing less sensitive to a drifting DC level.
// Longer windows are gentler (lower effective high-pass cutoff); shorter windows
// remove faster baseline variations but can alter the wanted waveform more.
static void remove_slow_baseline(WavData &wav, double window_ms) {
    if (wav.samples.empty() || wav.sample_rate == 0) return;

    uint32_t window = static_cast<uint32_t>(
        std::lround(static_cast<double>(wav.sample_rate) * window_ms / 1000.0));
    if (window < 5) window = 5;
    if ((window & 1U) == 0) ++window;
    const uint32_t half = window / 2;

    std::vector<int64_t> prefix(wav.samples.size() + 1, 0);
    for (size_t i = 0; i < wav.samples.size(); ++i) {
        prefix[i + 1] = prefix[i] + wav.samples[i];
    }

    std::vector<sample_t> corrected(wav.samples.size());
    for (size_t i = 0; i < wav.samples.size(); ++i) {
        const size_t lo = (i > half) ? i - half : 0;
        const size_t hi = std::min(wav.samples.size(), i + half + 1);
        const double baseline = static_cast<double>(prefix[hi] - prefix[lo]) /
                                static_cast<double>(hi - lo);
        double v = static_cast<double>(wav.samples[i]) - baseline;
        v = std::max(-32768.0, std::min(32767.0, v));
        corrected[i] = static_cast<sample_t>(std::lround(v));
    }

    wav.samples.swap(corrected);
}

struct Sample {
    sample_t s = 0;
    double scaled = 0.0;
    uint32_t index = 0;
    bool peak = false;
    bool high_peak = false;
};

using SampleBuffer = std::vector<Sample>;

struct Peak {
    sample_t value = 0;
    bool high = false;
    uint32_t buffer_index = 0;
    uint32_t index = 0;
    double scaled = 0.0;
};

using Peaks = std::vector<Peak>;

struct HiLo {
    sample_t high_value = 0;
    sample_t low_value = 0;
};

static HiLo find_highest_lowest(const SampleBuffer &sb) {
    HiLo h;
    h.high_value = sb.front().s;
    h.low_value = sb.front().s;

    for (const Sample &s : sb) {
        h.high_value = std::max(h.high_value, s.s);
        h.low_value = std::min(h.low_value, s.s);
    }
    return h;
}

static void scale_buffer(SampleBuffer &sb, sample_t low, sample_t high) {
    const double range = static_cast<double>(high) - static_cast<double>(low);

    // A completely flat window contains no useful transition information.
    if (std::fabs(range) < 1.0) {
        for (Sample &s : sb) {
            s.scaled = 0.5;
        }
        return;
    }

    for (Sample &s : sb) {
        s.scaled = (static_cast<double>(s.s) - static_cast<double>(low)) / range;
    }
}

static Peaks find_peaks(SampleBuffer &sb) {
    Peaks peaks;
    if (sb.size() < 4) {
        return peaks;
    }

    // This is the same local derivative test used by the newer decoder in
    // dpwav2tap.cpp: a sign change in the first difference marks a peak.
    for (size_t i = 1; i + 2 < sb.size(); ++i) {
        int32_t diff1 = static_cast<int32_t>(sb[i + 1].s) - sb[i].s;
        int32_t diff2 = static_cast<int32_t>(sb[i + 2].s) - sb[i + 1].s;

        bool is_peak = false;
        bool high = false;
        if (diff1 < 0 && diff2 >= 0) {
            is_peak = true;
            high = false;
        } else if (diff1 > 0 && diff2 <= 0) {
            is_peak = true;
            high = true;
        }

        if (is_peak) {
            sb[i + 1].peak = true;
            sb[i + 1].high_peak = high;
            Peak p;
            p.value = sb[i + 1].s;
            p.high = high;
            p.buffer_index = static_cast<uint32_t>(i + 1);
            p.index = sb[i + 1].index;
            p.scaled = sb[i + 1].scaled;
            peaks.push_back(p);
        }
    }
    return peaks;
}

struct PeakScore {
    uint32_t first_buffer_index = 0;
    uint32_t last_buffer_index = 0;
    uint32_t first_index = 0;
    uint32_t last_index = 0;
    double score = std::numeric_limits<double>::infinity();
};

static std::vector<PeakScore> score_acquisition_pairs(const Peaks &peaks,
                                                       double samples_per_bit) {
    std::vector<PeakScore> scores;

    for (size_t i = 0; i < peaks.size(); ++i) {
        for (size_t j = i + 1; j < peaks.size(); ++j) {
            // Consecutive useful extrema must have opposite polarity.
            if (peaks[i].high == peaks[j].high) {
                continue;
            }

            const double time_distance = static_cast<double>(peaks[j].index - peaks[i].index);
            const double amplitude_distance = std::fabs(peaks[i].scaled - peaks[j].scaled);
            const double time_score = std::pow((time_distance - samples_per_bit) / samples_per_bit, 2.0);
            const double amplitude_score = std::pow(amplitude_distance - 1.0, 2.0);

            PeakScore s;
            s.first_buffer_index = peaks[i].buffer_index;
            s.last_buffer_index = peaks[j].buffer_index;
            s.first_index = peaks[i].index;
            s.last_index = peaks[j].index;
            s.score = time_score + amplitude_score;
            scores.push_back(s);
        }
    }

    std::sort(scores.begin(), scores.end(),
              [](const PeakScore &a, const PeakScore &b) { return a.score < b.score; });
    return scores;
}

static uint32_t find_knee(const SampleBuffer &sb, uint32_t peak_buffer_index) {
    const double peak = sb[peak_buffer_index].scaled;
    const double previous_peak = sb.front().scaled;
    const double distance = std::fabs(peak - previous_peak);

    if (distance < 1.0e-9) {
        return sb[peak_buffer_index].index;
    }

    uint32_t i = peak_buffer_index;
    while (i > 0) {
        // Keep the original 0.925 threshold.  Walking backwards from the
        // extremum finds the front-porch "knee" used as transition time.
        if ((std::fabs(sb[i].scaled - previous_peak) / distance) < 0.925) {
            break;
        }
        --i;
    }
    return sb[i].index;
}

static SampleBuffer make_buffer(const std::vector<sample_t> &samples,
                                uint32_t start,
                                uint32_t count) {
    SampleBuffer sb;
    if (start >= samples.size()) {
        return sb;
    }

    const uint32_t end = static_cast<uint32_t>(
        std::min<size_t>(samples.size(), static_cast<size_t>(start) + count));
    sb.reserve(end - start);

    for (uint32_t i = start; i < end; ++i) {
        Sample s;
        s.s = samples[i];
        s.index = i;
        sb.push_back(s);
    }
    return sb;
}

struct BitEvent {
    uint32_t sample_index = 0;
    char bit = '0';
    std::string classification;
    double pll_period = 0.0;
};

class BitWriter {
public:
    BitWriter(FILE *bits_fp, FILE *trace_fp)
        : bits_fp_(bits_fp), trace_fp_(trace_fp) {
        if (trace_fp_) {
            fprintf(trace_fp_, "bit_index,sample_index,bit,classification,pll_period\n");
        }
    }

    void put(uint32_t sample_index, char bit, const char *classification,
             double pll_period) {
        BitEvent e;
        e.sample_index = sample_index;
        e.bit = bit;
        e.classification = classification;
        e.pll_period = pll_period;
        events_.push_back(e);

        if (trace_fp_) {
            fprintf(trace_fp_, "%llu,%u,%c,%s,%.6f\n",
                    static_cast<unsigned long long>(events_.size() - 1),
                    sample_index, bit, classification, pll_period);
        }
    }

    void finish() {
        if (bits_fp_) {
            format_framed_output();
        }
    }

    uint64_t bit_count() const { return events_.size(); }
    const std::vector<BitEvent> &events() const { return events_; }

private:
    static constexpr unsigned SYNC_THRESHOLD = 32;

    void write_char(char c) {
        if (bits_fp_) {
            fputc(c, bits_fp_);
        }
    }

    void write_range(size_t first, size_t last) {
        for (size_t i = first; i < last; ++i) {
            write_char(events_[i].bit);
        }
    }

    std::string bits_string(size_t first, size_t count) const {
        std::string s;
        s.reserve(count);
        for (size_t i = 0; i < count && first + i < events_.size(); ++i) {
            s.push_back(events_[first + i].bit);
        }
        return s;
    }

    void framing_error(size_t bit_index, const char *reason) const {
        // The integrated TAP/block processor owns error reporting. The optional
        // .bits formatter only marks questionable regions with '<'.
        (void)bit_index;
        (void)reason;
    }

    void format_framed_output() {
        size_t i = 0;
        bool have_output = false;

        while (i < events_.size()) {
            // Find a credible block start: a leader of more than 32 one bits
            // immediately followed by the initial 010 sync.
            const size_t leader_start = i;
            size_t p = i;
            while (p < events_.size() && events_[p].bit == '1') {
                ++p;
            }
            const size_t ones = p - i;

            if (ones > SYNC_THRESHOLD && p + 2 < events_.size() &&
                events_[p].bit == '0' && events_[p + 1].bit == '1' &&
                events_[p + 2].bit == '0') {
                // Leader/trailer runs are not wrapped. A single newline is
                // inserted at the boundary so every framed byte begins at
                // column zero.
                write_range(leader_start, p);
                if (have_output || p > leader_start) {
                    write_char('\n');
                }
                have_output = true;

                size_t prefix = p;
                for (;;) {
                    // Physical layout is:
                    //   sync0 data0 sync1 data1 ... syncN gap(11 ones)
                    // For inspection we display each byte as sync + data.
                    // Therefore the sync at 'prefix' belongs to the following
                    // eight data bits.
                    if (prefix + 14 <= events_.size()) {
                        bool gap = true;
                        for (size_t k = prefix + 3; k < prefix + 14; ++k) {
                            if (events_[k].bit != '1') {
                                gap = false;
                                break;
                            }
                        }
                        if (gap) {
                            // This is the final trailing sync followed by the
                            // 11-one end-of-record marker. Keep both unwrapped.
                            write_range(prefix, prefix + 14);
                            i = prefix + 14;
                            have_output = true;
                            break;
                        }
                    }

                    if (prefix + 11 > events_.size()) {
                        framing_error(prefix, "end of input inside framed byte");
                        write_range(prefix, events_.size());
                        i = events_.size();
                        have_output = true;
                        break;
                    }

                    bool mark_line = false;

                    const bool sync_valid =
                        events_[prefix].bit == '0' &&
                        events_[prefix + 1].bit == '1' &&
                        events_[prefix + 2].bit == '0';

                    if (!sync_valid) {
                        const std::string sync = bits_string(prefix, 3);

                        // A single missing/extra decoded bit would otherwise
                        // make every following line appear to have bad framing.
                        // Search forward for a strong resynchronization point:
                        // four 010 syncs at the normal 11-bit byte spacing.
                        size_t resync = events_.size();
                        const size_t search_end =
                            std::min(events_.size(), prefix + 256);
                        for (size_t q = prefix + 1; q + 36 <= search_end; ++q) {
                            bool strong = true;
                            for (size_t n = 0; n < 4; ++n) {
                                const size_t r = q + 11 * n;
                                if (events_[r].bit != '0' ||
                                    events_[r + 1].bit != '1' ||
                                    events_[r + 2].bit != '0') {
                                    strong = false;
                                    break;
                                }
                            }
                            if (strong) {
                                resync = q;
                                break;
                            }
                        }

                        if (resync != events_.size()) {

                            // Preserve the questionable decoded region exactly
                            // as recovered so it can be compared with the WAV
                            // and hand-edited. Prefix it with '<' so it stands
                            // out immediately in the editable .bits file.
                            write_char('<');
                            write_range(prefix, resync);
                            write_char('\n');
                            prefix = resync;
                            continue;
                        }

                        char msg[160];
                        snprintf(msg, sizeof(msg),
                                 "expected byte sync 010, got %s; unable to "
                                 "resynchronize within 256 bits",
                                 sync.c_str());
                        framing_error(prefix, msg);
                        mark_line = true;
                    }

                    bool data_valid = true;
                    for (size_t k = prefix + 3; k < prefix + 11; ++k) {
                        if (events_[k].bit != '0' && events_[k].bit != '1') {
                            data_valid = false;
                            break;
                        }
                    }
                    if (!data_valid) {
                        framing_error(prefix + 3,
                                      "undecidable waveform bit inside data byte");
                        mark_line = true;
                    }

                    // Always show the actual three framing bits followed by
                    // the eight recorded data bits. Thus a damaged sync is
                    // visible directly in the .bits file (for example 011...),
                    // while valid lines are exactly 010XXXXXXXX. Prefix a
                    // problematic line with '<' for easy manual inspection.
                    if (mark_line) {
                        write_char('<');
                    }
                    write_range(prefix, prefix + 11);
                    write_char('\n');
                    have_output = true;

                    // The next physical sync begins immediately after these
                    // eight data bits.
                    prefix += 11;
                }
                continue;
            }

            // No block start here. Preserve the unframed bits verbatim and
            // continue searching. This covers leaders, trailers and unrelated
            // signal without adding wrapping.
            write_range(leader_start, p);
            have_output = have_output || (p > leader_start);
            if (p < events_.size()) {
                write_char(events_[p].bit);
                have_output = true;
                i = p + 1;
            } else {
                i = p;
            }
        }
    }

    FILE *bits_fp_ = nullptr;
    FILE *trace_fp_ = nullptr;
    std::vector<BitEvent> events_;
};

class TransitionDecoder {
public:
    TransitionDecoder(double nominal_period, BitWriter &writer)
        : nominal_period_(nominal_period), pll_period_(nominal_period),
          min_period_(nominal_period * 0.75), max_period_(nominal_period * 1.25),
          writer_(writer) {}

    void reset() {
        state_ = INIT;
        pll_period_ = nominal_period_;
        prev_time_ = 0;
        half_time_ = 0;
    }

    void transition(uint32_t time) {
        switch (state_) {
        case INIT:
            prev_time_ = time;
            state_ = READY;
            return;

        case HALF_BIT: {
            int type = classify(time - half_time_);
            if (type == 0) {
                // Two short half-periods form a zero bit.
                emit(time, '0', "zero");
            } else if (type == -1) {
                emit(time, '?', "runt_second_half");
            } else if (type == 1) {
                // We already saw a credible short first half-period. On old
                // cassette recordings the opposite excursion can be flattened
                // or stretched by a local dropout, causing the second half to
                // look like a full-period interval. This is still most likely
                // a zero bit. Use twice the shorter half for the PLL update so
                // the damaged excursion cannot pull the PLL far off frequency.
                emit(time, '0', "zero_stretched_second_half");
            } else {
                emit(time, '?', "too_long_second_half");
            }

            if (type == 1) {
                const uint32_t first_half = half_time_ - prev_time_;
                const uint32_t second_half = time - half_time_;
                pll(2U * std::min(first_half, second_half));
            } else {
                pll(time - prev_time_);
            }
            prev_time_ = time;
            state_ = READY;
            return;
        }

        case READY: {
            int type = classify(time - prev_time_);
            if (type == 0) {
                // First short half-period; wait for its partner.
                half_time_ = time;
                state_ = HALF_BIT;
            } else if (type == 1) {
                // One full-period transition interval is a one bit.
                emit(time, '1', "one");
                pll(time - prev_time_);
                prev_time_ = time;
            } else if (type == -1) {
                emit(time, '?', "runt");
                half_time_ = time;
            } else {
                emit(time, '?', "too_long");
                pll(time - prev_time_);
                prev_time_ = time;
            }
            return;
        }
        }
    }

    // Score a prospective transition using its knee position and the
    // current PLL state. This is more reliable than measuring extremum-to-
    // extremum distance when a damaged waveform creates extra/weak peaks.
    double candidate_timing_score(uint32_t time) const {
        uint32_t reference = (state_ == HALF_BIT) ? half_time_ : prev_time_;
        if (time <= reference) {
            return std::numeric_limits<double>::infinity();
        }

        const double duration = static_cast<double>(time - reference);
        const double short_period = pll_period_ / 2.0;
        const double long_period = pll_period_;

        if (state_ == HALF_BIT) {
            // Once the first half of a zero has been accepted, strongly
            // prefer another short half-period.
            return std::pow(std::fabs(duration - short_period) / short_period, 3.0);
        }

        // In READY state either a short first half of zero or one complete
        // one-bit period is legitimate.
        const double short_score =
            std::pow(std::fabs(duration - short_period) / short_period, 3.0);
        const double long_score =
            std::pow(std::fabs(duration - long_period) / long_period, 3.0);
        return std::min(short_score, long_score);
    }

    double pll_period() const { return pll_period_; }

private:
    enum State { INIT, HALF_BIT, READY };

    int classify(uint32_t duration) const {
        if (duration < 0.25 * pll_period_) {
            return -1;
        }
        if (duration < 0.75 * pll_period_) {
            return 0;
        }
        if (duration < 1.50 * pll_period_) {
            return 1;
        }
        return 2;
    }

    void pll(uint32_t duration) {
        // Same first-order PLL correction as dpwav2tap.cpp.
        constexpr double pll_bump = 0.15;
        const double phase_diff = static_cast<double>(duration) - pll_period_;
        pll_period_ += phase_diff * pll_bump;
        pll_period_ = std::max(min_period_, std::min(max_period_, pll_period_));
    }

    void emit(uint32_t time, char bit, const char *why) {
        writer_.put(time, bit, why, pll_period_);
        vprint(3, "sample %u: bit %c (%s), pll %.3f\n", time, bit, why, pll_period_);
    }

    State state_ = INIT;
    double nominal_period_ = 0.0;
    double pll_period_ = 0.0;
    double min_period_ = 0.0;
    double max_period_ = 0.0;
    uint32_t prev_time_ = 0;
    uint32_t half_time_ = 0;
    BitWriter &writer_;
};

struct Options {
    const char *input = nullptr;
    std::string tap_output;
    std::string bits_output;
    std::string trace;
    std::string log;
    double dc_filter_ms = 0.0;
    bool dc_filter_auto = false;
};

static std::string replace_suffix(const std::string &input, const char *suffix) {
    std::string out = input;
    const size_t slash = out.find_last_of("/\\");
    const size_t dot = out.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        out.resize(dot);
    }
    out += suffix;
    return out;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [-v N] [-o output.tap] [-b output.bits] [-t trace.csv] [-l log.txt] [--dc-filter MS|auto] input.wav\n"
            "\n"
            "  -o FILE   output SIMH TAP file (default: input.tap)\n"
            "  -b FILE   optional editable framed bit output\n"
            "  -t FILE   optional raw bit timing/PLL CSV trace\n"
            "  -l FILE   block/framing log (default: stderr)\n"
            "  -v N      waveform diagnostic verbosity, 0..3\n"
            "  --dc-filter MS    subtract a centered MS-millisecond moving average before peak detection\n"
            "  --dc-filter auto  decode unfiltered first, then use alternate windows only to fill failed block gaps\n"
            "\n"
            "Only complete framing-valid blocks are considered for TAP output.\n"
            "Numeric records must also pass their Datapoint checksum.\n",
            prog);
    std::exit(1);
}

static Options parse_args(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            verbosity = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            o.tap_output = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            o.bits_output = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            o.trace = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            o.log = argv[++i];
        } else if (strcmp(argv[i], "--dc-filter") == 0 && i + 1 < argc) {
            const char *arg = argv[++i];
            if (strcmp(arg, "auto") == 0) {
                o.dc_filter_auto = true;
                o.dc_filter_ms = 0.0;
            } else {
                char *end = nullptr;
                const double ms = std::strtod(arg, &end);
                if (!end || *end != '\0' || !std::isfinite(ms) || ms <= 0.0) {
                    fprintf(stderr, "Invalid --dc-filter window '%s'; expected a positive number of milliseconds or 'auto'\n", arg);
                    usage(argv[0]);
                }
                o.dc_filter_ms = ms;
            }
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
        } else if (!o.input) {
            o.input = argv[i];
        } else {
            usage(argv[0]);
        }
    }
    if (!o.input) usage(argv[0]);
    if (o.tap_output.empty()) o.tap_output = replace_suffix(o.input, ".tap");
    return o;
}

static uint8_t reverse_byte(uint8_t value) {
    value = static_cast<uint8_t>(((value & 0x55U) << 1) | ((value & 0xAAU) >> 1));
    value = static_cast<uint8_t>(((value & 0x33U) << 2) | ((value & 0xCCU) >> 2));
    return static_cast<uint8_t>((value << 4) | (value >> 4));
}

static bool write_u32_le(FILE *fp, uint32_t value) {
    const uint8_t b[4] = {
        static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)
    };
    return fwrite(b, 1, sizeof(b), fp) == sizeof(b);
}

static bool write_tap_record(FILE *fp, const std::vector<uint8_t> &data) {
    if (data.empty() || data.size() > UINT32_MAX) return false;
    const uint32_t len = static_cast<uint32_t>(data.size());
    return write_u32_le(fp, len) &&
           fwrite(data.data(), 1, data.size(), fp) == data.size() &&
           write_u32_le(fp, len);
}

static bool is_file_header(const std::vector<uint8_t> &b) {
    return b.size() >= 2 && b[0] == 0201 && b[1] == 0176;
}
static bool is_numeric_record(const std::vector<uint8_t> &b) {
    return b.size() >= 2 && b[0] == 0303 && b[1] == 0074;
}
static bool is_symbolic_record(const std::vector<uint8_t> &b) {
    return b.size() >= 2 && b[0] == 0347 && b[1] == 0030;
}

static bool datapoint_checksum_ok(const std::vector<uint8_t> &b) {
    if (b.size() < 4) return false;
    uint8_t xor_checksum = b[2];
    uint8_t circulated_checksum = b[3];
    for (size_t i = 4; i < b.size(); ++i) {
        xor_checksum ^= b[i];
        circulated_checksum ^= b[i];
        const uint8_t lowest = circulated_checksum & 1U;
        circulated_checksum >>= 1;
        circulated_checksum |= static_cast<uint8_t>(lowest << 7);
    }
    return xor_checksum == 0 && circulated_checksum == 0;
}

struct BlockStats {
    // synced_blocks counts every leader + initial 010 acquisition.  Some of
    // these are write-splice/noise false acquisitions and never become a
    // confirmed block.  Keep separate counters so the summary is auditable.
    uint64_t synced_blocks = 0;
    uint64_t completed_blocks = 0;
    uint64_t empty_blocks = 0;
    uint64_t false_first_byte_acquisitions = 0;
    uint64_t framing_bad = 0;
    uint64_t content_bad = 0;
    uint64_t written = 0;
    uint64_t bytes_written = 0;
};

struct BlockCandidate {
    uint32_t start_sample = 0;
    uint32_t end_sample = 0;
    std::vector<uint8_t> data;
    double filter_ms = 0.0;
    bool numeric = false;
    bool file_header = false;
    bool symbolic = false;
};

struct FailedBlock {
    enum class Kind { FRAMING, CONTENT };
    Kind kind = Kind::FRAMING;
    uint64_t block_number = 0;
    uint32_t start_sample = 0;
    uint32_t end_sample = 0;
    size_t decoded_bytes = 0;
};

class BlockProcessor {
public:
    BlockProcessor(FILE *tap, FILE *log, BlockStats &stats,
                   std::vector<BlockCandidate> *candidates = nullptr,
                   double filter_ms = 0.0,
                   std::vector<FailedBlock> *failures = nullptr)
        : tap_(tap), log_(log), stats_(stats), candidates_(candidates),
          filter_ms_(filter_ms), failures_(failures) {}

    bool process(const std::vector<BitEvent> &events) {
        events_ = &events;
        for (size_t i = 0; i < events.size(); ++i) {
            const BitEvent &e = events[i];
            const int bit = (e.bit == '1') ? 1 : (e.bit == '0' ? 0 : -1);

            // Ignore physical-decoder errors until block synchronization has
            // been acquired. Write splices and inter-record gaps are expected
            // to look ugly and are not useful diagnostics.
            if (bit < 0) {
                if (state_ == State::BLOCK) {
                    if (!try_erasure_recovery(i, i, "undecidable waveform bit", i)) {
                        fprintf(log_,
                                "Block %llu lost sync: undecidable bit %zu at WAV sample %u (%s); "
                                "discarding %zu decoded byte%s\n",
                                static_cast<unsigned long long>(block_number_), i,
                                e.sample_index, e.classification.c_str(), block_.size(),
                                block_.size() == 1 ? "" : "s");
                        ++stats_.framing_bad;
                        record_failure(FailedBlock::Kind::FRAMING, e.sample_index, block_.size());
                        lose_sync();
                    } else {
                        i = skip_to_bit_ - 1;
                    }
                }
                continue;
            }

            switch (state_) {
            case State::SEARCH:
                if (bit) ++ones_;
                else if (ones_ > SYNC_THRESHOLD) state_ = State::PREAMBLE_0;
                else ones_ = 0;
                break;

            case State::PREAMBLE_0:
                if (bit) state_ = State::PREAMBLE_01;
                else { state_ = State::SEARCH; ones_ = 0; }
                break;

            case State::PREAMBLE_01:
                if (!bit) {
                    ++block_number_;
                    ++stats_.synced_blocks;
                    block_start_bit_ = i >= 2 ? i - 2 : 0;
                    block_start_sample_ = events[block_start_bit_].sample_index;
                    block_.clear();
                    erasures_.clear();
                    frame_bits_ = 0;
                    frame_count_ = 0;
                    state_ = State::BLOCK;
                    fprintf(log_, "Block %llu sync at bit %zu, WAV sample %u\n",
                            static_cast<unsigned long long>(block_number_),
                            block_start_bit_, block_start_sample_);
                } else {
                    state_ = State::SEARCH;
                    ones_ = 2;
                }
                break;

            case State::BLOCK:
                if (!feed_block_bit(bit, i, e.sample_index, i)) return false;
                break;
            }
        }

        if (state_ == State::BLOCK) {
            fprintf(log_,
                    "Block %llu lost sync: end of WAV after %zu decoded byte%s; block discarded\n",
                    static_cast<unsigned long long>(block_number_), block_.size(),
                    block_.size() == 1 ? "" : "s");
            ++stats_.framing_bad;
            if (!block_.empty() && !events.empty()) {
                record_failure(FailedBlock::Kind::FRAMING, events.back().sample_index, block_.size());
            }
        }
        return true;
    }

private:
    enum class State { SEARCH, PREAMBLE_0, PREAMBLE_01, BLOCK };
    static constexpr size_t SYNC_THRESHOLD = 32;

    struct Erasure {
        size_t first_byte = 0;
        size_t count = 0;
        size_t start_bit = 0;
        size_t resync_bit = 0;
        uint32_t start_sample = 0;
        std::string observed_bits;
        bool starts_with_sync = true;
    };

    State state_ = State::SEARCH;
    FILE *tap_ = nullptr;
    FILE *log_ = nullptr;
    BlockStats &stats_;
    std::vector<BlockCandidate> *candidates_ = nullptr;
    double filter_ms_ = 0.0;
    std::vector<FailedBlock> *failures_ = nullptr;
    const std::vector<BitEvent> *events_ = nullptr;
    size_t ones_ = 0;
    uint64_t block_number_ = 0;
    size_t block_start_bit_ = 0;
    uint32_t block_start_sample_ = 0;
    uint16_t frame_bits_ = 0;
    unsigned frame_count_ = 0;
    std::vector<uint8_t> block_;
    std::vector<Erasure> erasures_;
    size_t skip_to_bit_ = 0;

    void record_failure(FailedBlock::Kind kind, uint32_t end_sample, size_t decoded_bytes) {
        if (!failures_) return;
        FailedBlock f;
        f.kind = kind;
        f.block_number = block_number_;
        f.start_sample = block_start_sample_;
        f.end_sample = end_sample;
        f.decoded_bytes = decoded_bytes;
        failures_->push_back(f);
    }

    void lose_sync() {
        state_ = State::SEARCH;
        ones_ = 0;
        frame_bits_ = 0;
        frame_count_ = 0;
        block_.clear();
        erasures_.clear();
    }

    static bool sync010(const std::vector<BitEvent> &events, size_t p) {
        return p + 2 < events.size() &&
               events[p].bit == '0' && events[p + 1].bit == '1' &&
               events[p + 2].bit == '0';
    }

    static bool strong_resync(const std::vector<BitEvent> &events, size_t p) {
        // Four consecutive byte syncs at 11-bit spacing are unlikely to be
        // accidental and provide a reliable phase reference after a dropout.
        for (size_t n = 0; n < 4; ++n) {
            if (!sync010(events, p + 11 * n)) return false;
        }
        return true;
    }

    bool try_erasure_recovery(size_t frame_start, size_t search_from,
                              const char *reason, size_t &loop_index,
                              bool starts_with_sync = true) {
        if (!events_ || block_.empty()) return false;
        const std::vector<BitEvent> &events = *events_;

        const size_t search_end = std::min(events.size(), frame_start + 256);
        size_t resync = events.size();
        for (size_t q = std::max(frame_start + 1, search_from + 1);
             q + 36 <= search_end; ++q) {
            if (strong_resync(events, q)) {
                resync = q;
                break;
            }
        }
        if (resync == events.size()) return false;

        const size_t decoded_span = resync - frame_start;
        const size_t missing_bytes = std::max<size_t>(1, (decoded_span + 5) / 11);

        // Keep recovery deliberately bounded. One or two erased bytes can be
        // solved cheaply and reliably from the 16-bit block checksum. Larger
        // damaged spans should remain manual-recovery cases.
        if (missing_bytes > 2) return false;

        Erasure er;
        er.first_byte = block_.size();
        er.count = missing_bytes;
        er.start_bit = frame_start;
        er.resync_bit = resync;
        er.start_sample = events[frame_start].sample_index;
        er.starts_with_sync = starts_with_sync;
        er.observed_bits.reserve(decoded_span);
        for (size_t k = frame_start; k < resync; ++k) {
            er.observed_bits.push_back(events[k].bit);
        }
        erasures_.push_back(er);

        // Placeholder bytes keep all subsequent byte positions correct. They
        // are filled only if checksum-guided recovery is unique/best later.
        block_.insert(block_.end(), missing_bytes, 0);
        frame_bits_ = 0;
        frame_count_ = 0;
        skip_to_bit_ = resync;
        loop_index = resync - 1;

        fprintf(log_,
                "Block %llu sync anomaly at bit %zu, WAV sample %u (%s); "
                "strong byte framing returns at bit %zu, WAV sample %u. "
                "Treating span as %zu erased byte%s for checksum recovery.\n",
                static_cast<unsigned long long>(block_number_), frame_start,
                events[frame_start].sample_index, reason, resync,
                events[resync].sample_index, missing_bytes,
                missing_bytes == 1 ? "" : "s");
        return true;
    }

    bool preceding_byte_timing_suspicious(size_t data_start) const {
        if (!events_ || data_start + 7 >= events_->size()) return false;
        const auto &events = *events_;

        // Establish a local PLL baseline from the preceding 16 decoded bits.
        // A real dropout often starts before the framing bits themselves and
        // pulls the PLL appreciably during the data byte. In that case the
        // apparently valid byte immediately before the bad sync should also
        // be treated as an erasure rather than trusted blindly.
        const size_t begin = data_start > 16 ? data_start - 16 : 0;
        std::vector<double> periods;
        periods.reserve(data_start - begin);
        for (size_t i = begin; i < data_start; ++i) {
            periods.push_back(events[i].pll_period);
        }
        if (periods.empty()) return false;
        std::sort(periods.begin(), periods.end());
        const double baseline = periods[periods.size() / 2];
        if (baseline <= 0.0) return false;

        double worst_relative = 0.0;
        for (size_t i = data_start; i < data_start + 8; ++i) {
            const double rel = std::fabs(events[i].pll_period - baseline) / baseline;
            worst_relative = std::max(worst_relative, rel);
            if (events[i].classification != "zero" &&
                events[i].classification != "one") {
                return true;
            }
        }

        // The known good case preceding a damaged sync varies by ~1%, while
        // the corrupted preceding byte in T44S144K exceeds 8%. Five percent
        // leaves substantial margin while still requiring a clear disturbance.
        return worst_relative > 0.05;
    }

    bool feed_block_bit(int bit, size_t bit_index, uint32_t sample_index,
                        size_t &loop_index) {
        frame_bits_ = static_cast<uint16_t>((frame_bits_ << 1) | unsigned(bit));
        ++frame_count_;
        if (frame_count_ < 11) return true;
        frame_bits_ &= 0x07ffU;

        if (frame_bits_ == 0x07ffU) {
            if (block_.empty()) {
                fprintf(log_, "Block %llu invalid: empty block ended at bit %zu, WAV sample %u\n",
                        static_cast<unsigned long long>(block_number_), bit_index, sample_index);
                ++stats_.empty_blocks;
                ++stats_.framing_bad;
            } else {
                if (!finish_block(bit_index, sample_index)) return false;
            }
            state_ = State::SEARCH;
            ones_ = 11;
            frame_bits_ = 0;
            frame_count_ = 0;
            block_.clear();
            erasures_.clear();
            return true;
        }

        if ((frame_bits_ & 7U) == 2U) {
            uint8_t value = static_cast<uint8_t>((frame_bits_ >> 3) & 0xffU);
            block_.push_back(reverse_byte(value));
            frame_bits_ = 0;
            frame_count_ = 0;
            return true;
        }

        // False acquisition due to a write splice: still suppress it when no
        // complete byte has yet been received.
        if (block_.empty()) {
            // A leader + initial 010 was seen, but the first complete byte did
            // not validate.  This is normally a write splice/noise acquisition
            // and is intentionally not counted as a framing error.
            ++stats_.false_first_byte_acquisitions;
            lose_sync();
            return true;
        }

        // The first eight bits of this 11-bit group are the data byte and
        // the last three are the sync. If only the sync is bad, preserve the
        // data byte: the damaged/recovery span starts at the first bad sync
        // bit, not eight bits earlier at the beginning of this byte.
        const uint8_t pending_value =
            reverse_byte(static_cast<uint8_t>((frame_bits_ >> 3) & 0xffU));
        const size_t sync_start = bit_index - 2;
        char reason[96];
        snprintf(reason, sizeof(reason), "expected sync 010, got %u%u%u",
                 unsigned((frame_bits_ >> 2) & 1U),
                 unsigned((frame_bits_ >> 1) & 1U),
                 unsigned(frame_bits_ & 1U));

        const size_t data_start = bit_index - 10;
        const bool pending_suspicious = preceding_byte_timing_suspicious(data_start);

        if (pending_suspicious) {
            // Distortion was already visible while decoding the eight data
            // bits before the failed sync. Do not preserve that byte; include
            // it in the erasure span and let the block checksum plus waveform
            // similarity recover it together with the following lost byte(s).
            if (try_erasure_recovery(data_start, bit_index, reason, loop_index,
                                     false)) {
                fprintf(log_,
                        "Block %llu: timing was already unstable in the byte before "
                        "the bad sync; including that byte in checksum recovery.\n",
                        static_cast<unsigned long long>(block_number_));
                skip_to_bit_ += 3;
                loop_index = skip_to_bit_ - 1;
                return true;
            }
        } else {
            block_.push_back(pending_value);
            if (try_erasure_recovery(sync_start, bit_index, reason, loop_index,
                                     true)) {
                // Resynchronization points at the next 010 prefix. Consume that
                // sync and resume with the following eight data bits.
                skip_to_bit_ += 3;
                loop_index = skip_to_bit_ - 1;
                return true;
            }
            block_.pop_back();
        }

        fprintf(log_,
                "Block %llu lost sync at bit %zu, WAV sample %u: expected byte sync 010, got %u%u%u "
                "after %zu valid byte%s; block discarded\n",
                static_cast<unsigned long long>(block_number_), bit_index, sample_index,
                unsigned((frame_bits_ >> 2) & 1U), unsigned((frame_bits_ >> 1) & 1U),
                unsigned(frame_bits_ & 1U), block_.size(), block_.size() == 1 ? "" : "s");
        ++stats_.framing_bad;
        record_failure(FailedBlock::Kind::FRAMING, sample_index, block_.size());
        lose_sync();
        return true;
    }

    static std::string tape_data_bits(uint8_t v) {
        std::string s;
        s.reserve(8);
        for (unsigned bit = 0; bit < 8; ++bit) {
            s.push_back((v & (1U << bit)) ? '1' : '0');
        }
        return s;
    }

    static std::string expected_erasure_bits(const std::vector<uint8_t> &bytes,
                                              const Erasure &er) {
        std::string s;
        if (er.count == 0) return s;

        if (er.starts_with_sync) {
            for (size_t n = 0; n < er.count; ++n) {
                s += "010";
                s += tape_data_bits(bytes[er.first_byte + n]);
            }
        } else {
            // The damaged span begins at the data bits of the preceding byte,
            // not at its leading sync. The resynchronization point is the
            // leading sync of the first known-good byte after the erasure.
            s += tape_data_bits(bytes[er.first_byte]);
            for (size_t n = 1; n < er.count; ++n) {
                s += "010";
                s += tape_data_bits(bytes[er.first_byte + n]);
            }
        }
        return s;
    }

    static unsigned edit_distance(const std::string &a, const std::string &b) {
        // Small Levenshtein distance implementation. Erasure regions are tiny,
        // so a simple dynamic-programming row is sufficient.
        std::vector<unsigned> prev(b.size() + 1), cur(b.size() + 1);
        for (size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<unsigned>(j);
        for (size_t i = 1; i <= a.size(); ++i) {
            cur[0] = static_cast<unsigned>(i);
            for (size_t j = 1; j <= b.size(); ++j) {
                const unsigned sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0U : 1U);
                cur[j] = std::min({prev[j] + 1U, cur[j - 1] + 1U, sub});
            }
            prev.swap(cur);
        }
        return prev[b.size()];
    }

    static uint16_t checksum_residue(const std::vector<uint8_t> &b) {
        if (b.size() < 4) return 0xffffU;
        uint8_t x = b[2];
        uint8_t c = b[3];
        for (size_t i = 4; i < b.size(); ++i) {
            x ^= b[i];
            c ^= b[i];
            const uint8_t lowest = c & 1U;
            c >>= 1;
            c |= static_cast<uint8_t>(lowest << 7);
        }
        return static_cast<uint16_t>((uint16_t(x) << 8) | c);
    }

    unsigned recovery_waveform_score(const std::vector<uint8_t> &candidate) const {
        unsigned score = 0;
        for (const Erasure &er : erasures_) {
            score += edit_distance(expected_erasure_bits(candidate, er), er.observed_bits);
        }
        return score;
    }

    unsigned recovery_context_score(const std::vector<uint8_t> &candidate,
                                    const std::vector<size_t> &unknown_positions) const {
        // Tertiary tie-breaker only. Old program tapes often contain long runs
        // of padding spaces or repeated structured bytes. Compare each erased
        // byte with nearby *known* bytes; a candidate matching the local pattern
        // receives a lower score. This never overrides checksum validity or a
        // better waveform edit score.
        std::vector<bool> unknown(candidate.size(), false);
        for (size_t p : unknown_positions) {
            if (p < unknown.size()) unknown[p] = true;
        }

        unsigned score = 0;
        constexpr size_t radius = 8;
        for (size_t p : unknown_positions) {
            const size_t first = p > radius ? p - radius : 0;
            const size_t last = std::min(candidate.size(), p + radius + 1);
            for (size_t q = first; q < last; ++q) {
                if (q == p || unknown[q]) continue;
                const unsigned distance = static_cast<unsigned>(p > q ? p - q : q - p);
                const unsigned xorv = static_cast<unsigned>(candidate[p] ^ candidate[q]);
#if defined(__GNUC__) || defined(__clang__)
                const unsigned bitdiff = static_cast<unsigned>(__builtin_popcount(xorv));
#else
                unsigned t = xorv, bitdiff = 0;
                while (t) { bitdiff += t & 1U; t >>= 1; }
#endif
                // Nearby bytes matter a little more than distant ones.
                score += bitdiff * (radius + 1 - distance);
            }
        }
        return score;
    }

    bool recover_erasures_from_checksum() {
        if (erasures_.empty()) return datapoint_checksum_ok(block_);

        std::vector<size_t> positions;
        for (const Erasure &er : erasures_) {
            if (er.count == 0 || er.first_byte + er.count > block_.size()) return false;
            for (size_t n = 0; n < er.count; ++n) {
                positions.push_back(er.first_byte + n);
            }
        }
        std::sort(positions.begin(), positions.end());
        positions.erase(std::unique(positions.begin(), positions.end()), positions.end());

        // Three unknown bytes still leave only ~256 checksum-valid solutions
        // because the checksum supplies 16 constraints. More than three is
        // intentionally left for manual recovery for now.
        if (positions.empty() || positions.size() > 3) {
            fprintf(log_, "Checksum recovery supports at most 3 erased bytes; got %zu. ",
                    positions.size());
            return false;
        }

        std::vector<uint8_t> base = block_;
        for (size_t p : positions) base[p] = 0;
        const uint16_t base_residue = checksum_residue(base);

        std::vector<std::vector<uint16_t>> effect(positions.size(),
                                                  std::vector<uint16_t>(256));
        for (size_t n = 0; n < positions.size(); ++n) {
            std::vector<uint8_t> tmp = base;
            for (unsigned v = 0; v < 256; ++v) {
                tmp[positions[n]] = static_cast<uint8_t>(v);
                effect[n][v] = static_cast<uint16_t>(base_residue ^ checksum_residue(tmp));
            }
        }

        bool found = false;
        bool tied = false;
        unsigned best_score = std::numeric_limits<unsigned>::max();
        unsigned best_context = std::numeric_limits<unsigned>::max();
        std::vector<uint8_t> best_values(positions.size(), 0);
        size_t solutions = 0;

        auto consider = [&](const std::vector<uint8_t> &values) {
            std::vector<uint8_t> candidate = base;
            for (size_t n = 0; n < positions.size(); ++n) {
                candidate[positions[n]] = values[n];
            }
            ++solutions;
            const unsigned score = recovery_waveform_score(candidate);
            const unsigned context = recovery_context_score(candidate, positions);
            if (!found || score < best_score ||
                (score == best_score && context < best_context)) {
                found = true;
                tied = false;
                best_score = score;
                best_context = context;
                best_values = values;
            } else if (score == best_score && context == best_context &&
                       values != best_values) {
                tied = true;
            }
        };

        if (positions.size() == 1) {
            for (unsigned a = 0; a < 256; ++a) {
                if ((base_residue ^ effect[0][a]) == 0) {
                    consider({static_cast<uint8_t>(a)});
                }
            }
        } else if (positions.size() == 2) {
            std::unordered_multimap<uint16_t, uint16_t> right;
            right.reserve(512);
            for (unsigned b = 0; b < 256; ++b) {
                right.emplace(effect[1][b], static_cast<uint16_t>(b));
            }
            for (unsigned a = 0; a < 256; ++a) {
                const uint16_t want = static_cast<uint16_t>(base_residue ^ effect[0][a]);
                auto range = right.equal_range(want);
                for (auto it = range.first; it != range.second; ++it) {
                    consider({static_cast<uint8_t>(a), static_cast<uint8_t>(it->second)});
                }
            }
        } else {
            // Meet in the middle: index the combined effects of the last two
            // bytes, then try each value of the first byte. This avoids a
            // 16-million full-block brute-force scan.
            std::unordered_multimap<uint16_t, uint16_t> pair_effects;
            pair_effects.reserve(70000);
            for (unsigned b = 0; b < 256; ++b) {
                for (unsigned c = 0; c < 256; ++c) {
                    const uint16_t key = static_cast<uint16_t>(effect[1][b] ^ effect[2][c]);
                    pair_effects.emplace(key, static_cast<uint16_t>((b << 8) | c));
                }
            }
            for (unsigned a = 0; a < 256; ++a) {
                const uint16_t want = static_cast<uint16_t>(base_residue ^ effect[0][a]);
                auto range = pair_effects.equal_range(want);
                for (auto it = range.first; it != range.second; ++it) {
                    const unsigned b = (it->second >> 8) & 0xffU;
                    const unsigned c = it->second & 0xffU;
                    consider({static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                              static_cast<uint8_t>(c)});
                }
            }
        }

        if (!found || tied) {
            fprintf(log_,
                    "Checksum recovery failed/ambiguous for %zu erased bytes "
                    "(%zu checksum solution%s, best waveform score %u, context score %u). ",
                    positions.size(), solutions, solutions == 1 ? "" : "s",
                    found ? best_score : 0U, found ? best_context : 0U);
            return false;
        }

        for (size_t n = 0; n < positions.size(); ++n) {
            block_[positions[n]] = best_values[n];
        }

        fprintf(log_, "Recovered %zu erased bytes from checksum/waveform:", positions.size());
        for (size_t n = 0; n < positions.size(); ++n) {
            fprintf(log_, " byte %zu=%02X", positions[n], unsigned(best_values[n]));
        }
        fprintf(log_, ". Waveform edit score %u, context score %u; "
                      "%zu checksum solution%s considered. ",
                best_score, best_context, solutions, solutions == 1 ? "" : "s");
        return datapoint_checksum_ok(block_);
    }

    bool finish_block(size_t end_bit, uint32_t end_sample) {
        ++stats_.completed_blocks;

        // Write policy:
        //   * Numeric records have a defined checksum and are written whenever
        //     that checksum is valid.  Structural/metadata problems (for
        //     example a record too short to contain a load address) are logged
        //     as warnings but do not suppress a checksum-valid record.
        //   * File headers, symbolic records and unknown/boot records have no
        //     checksum rule in the supplied Datapoint checker.  Preserve them
        //     in the TAP output and report any structural problems as warnings.
        bool content_ok = true;
        fprintf(log_, "Block %llu complete: bits %zu..%zu, WAV samples %u..%u, %zu bytes. ",
                static_cast<unsigned long long>(block_number_), block_start_bit_, end_bit,
                block_start_sample_, end_sample, block_.size());

        if (is_file_header(block_)) {
            fprintf(log_, "FileHeader. ");
            if (!erasures_.empty()) {
                fprintf(log_, "WARNING: contains erased byte(s); no checksum is defined for this block type. ");
            }
            if (block_.size() != 4) {
                fprintf(log_, "WARNING: BAD size (expected 4). ");
            }
            if (block_.size() >= 4) {
                fprintf(log_, "File number %u. ", unsigned(block_[2]));
                if (block_[2] != static_cast<uint8_t>(~block_[3])) {
                    fprintf(log_, "WARNING: BAD inverted file number. ");
                }
                if (block_[2] == 127) fprintf(log_, "End-of-tape marker. ");
            }
            fprintf(log_, "No checksum defined by supplied checker. ");
        } else if (is_numeric_record(block_)) {
            fprintf(log_, "Numeric record. ");
            bool checksum_ok = datapoint_checksum_ok(block_);
            if (!checksum_ok && !erasures_.empty()) {
                checksum_ok = recover_erasures_from_checksum();
            }
            fprintf(log_, "Checksum %s. ", checksum_ok ? "OK" : "BAD");
            if (!checksum_ok) content_ok = false;
            if (block_.size() >= 8) {
                const unsigned addr = (unsigned(block_[4]) << 8) | block_[5];
                fprintf(log_, "Load address %05o. ", addr);
                if (block_[4] != static_cast<uint8_t>(~block_[6]) ||
                    block_[5] != static_cast<uint8_t>(~block_[7])) {
                    fprintf(log_, "WARNING: bad load-address complement. ");
                }
            } else {
                fprintf(log_, "WARNING: BAD size for load-address fields. ");
            }
        } else if (is_symbolic_record(block_)) {
            fprintf(log_, "Symbolic record. ");
            if (!erasures_.empty()) {
                fprintf(log_, "WARNING: contains erased byte(s); no checksum recovery is defined. ");
            }
        } else {
            fprintf(log_, "Unknown/boot block. ");
            if (!erasures_.empty()) {
                fprintf(log_, "WARNING: contains erased byte(s); no checksum recovery is defined. ");
            }
        }

        if (!content_ok) {
            fprintf(log_, "NOT written to TAP.\n");
            ++stats_.content_bad;
            record_failure(FailedBlock::Kind::CONTENT, end_sample, block_.size());
            return true;
        }

        if (candidates_) {
            BlockCandidate c;
            c.start_sample = block_start_sample_;
            c.end_sample = end_sample;
            c.data = block_;
            c.filter_ms = filter_ms_;
            c.numeric = is_numeric_record(block_);
            c.file_header = is_file_header(block_);
            c.symbolic = is_symbolic_record(block_);
            candidates_->push_back(std::move(c));
        }

        if (tap_) {
            if (!write_tap_record(tap_, block_)) {
                fprintf(log_, "TAP write FAILED.\n");
                return false;
            }
            fprintf(log_, "Written to TAP.\n");
        } else {
            fprintf(log_, "Accepted as block candidate.\n");
        }
        ++stats_.written;
        stats_.bytes_written += block_.size();
        return true;
    }
};

static void decode(const WavData &wav, BitWriter &writer) {
    // The existing program uses 969.94 Hz for the long transition interval
    // on recordings played at 1 7/8 ips.  Scale it by the actual WAV rate.
    constexpr double zero_freq = 969.94;
    const double nominal_period = static_cast<double>(wav.sample_rate) / zero_freq;

    TransitionDecoder decoder(nominal_period, writer);

    bool locked = false;
    bool last_peak_high = false;
    uint32_t last_peak = 0;
    uint32_t scan = 0;

    const uint32_t acquisition_window = 80;
    const uint32_t tracking_window = 67;

    while (scan < wav.samples.size()) {
        if (!locked) {
            SampleBuffer sb = make_buffer(wav.samples, scan, acquisition_window);
            if (sb.size() < 4) {
                break;
            }

            HiLo hl = find_highest_lowest(sb);
            scale_buffer(sb, hl.low_value, hl.high_value);
            Peaks peaks = find_peaks(sb);
            auto scores = score_acquisition_pairs(peaks, nominal_period);

            if (scores.empty()) {
                // Move forward by half a window so weak/dropout regions do not
                // cause us to get stuck while still retaining overlap.
                scan += acquisition_window / 2;
                continue;
            }

            const PeakScore &best = scores.front();

            // Reject extremely implausible acquisition pairs.  The original
            // code always took the best score; this loose limit merely avoids
            // locking to an almost-flat noise window.
            if (best.score > 1.5) {
                scan += acquisition_window / 2;
                continue;
            }

            decoder.reset();
            decoder.transition(best.first_index);
            decoder.transition(best.last_index);

            last_peak = best.last_index;
            last_peak_high = sb[best.last_buffer_index].high_peak;
            locked = true;
            scan = last_peak;

            vprint(2, "lock at samples %u..%u, score %.4f\n",
                   best.first_index, best.last_index, best.score);
            continue;
        }

        SampleBuffer sb = make_buffer(wav.samples, last_peak, tracking_window);
        if (sb.size() < 4) {
            break;
        }

        HiLo hl = find_highest_lowest(sb);
        const double range = static_cast<double>(hl.high_value) - hl.low_value;
        if (std::fabs(range) < 8.0) {
            // Near-silence/dropout: explicitly mark loss and reacquire later.
            writer.put(last_peak, '?', "dropout", decoder.pll_period());
            locked = false;
            scan = last_peak + tracking_window / 2;
            vprint(2, "signal dropout near sample %u\n", last_peak);
            continue;
        }

        scale_buffer(sb, hl.low_value, hl.high_value);
        Peaks peaks = find_peaks(sb);

        // Score each opposite-polarity peak by the timing of its front-edge
        // knee relative to the decoder's live PLL state.  The previous code
        // used the extremum's buffer offset, which can skip a real half-bit
        // when a damaged waveform produces two nearly equal candidates.
        std::vector<PeakScore> scores;
        for (const Peak &p : peaks) {
            if (p.high == last_peak_high) {
                continue;
            }

            const uint32_t knee = find_knee(sb, p.buffer_index);
            const double timing_score = decoder.candidate_timing_score(knee);
            const double amplitude_distance =
                std::fabs(p.scaled - sb.front().scaled);
            const double amplitude_score =
                1.0 / std::max(amplitude_distance, 1.0e-9);

            PeakScore ps;
            ps.first_buffer_index = p.buffer_index;
            ps.first_index = p.index;
            ps.score = 8.0 * timing_score + 0.10 * amplitude_score;
            scores.push_back(ps);
        }

        std::sort(scores.begin(), scores.end(),
                  [](const PeakScore &a, const PeakScore &b) {
                      return a.score < b.score;
                  });

        if (scores.empty() || scores.front().score > 8.0) {
            // Do not create a bogus bit merely because the WAV ends in the
            // middle of the final carrier cycle.
            if (last_peak + tracking_window >= wav.samples.size()) {
                break;
            }

            // Away from EOF, preserve a diagnostic marker for now.
            writer.put(last_peak, '?', "lost_peak", decoder.pll_period());
            locked = false;
            scan = last_peak + tracking_window / 2;
            vprint(2, "lost peak tracking near sample %u\n", last_peak);
            continue;
        }

        const PeakScore &best = scores.front();
        const uint32_t peak_buffer_index = best.first_buffer_index;
        const uint32_t knee = find_knee(sb, peak_buffer_index);

        last_peak_high = sb[peak_buffer_index].high_peak;
        last_peak = best.first_index;
        decoder.transition(knee);
        scan = last_peak;
    }

    writer.finish();
}

struct AttemptResult {
    double filter_ms = 0.0;
    BlockStats stats;
    uint64_t bit_count = 0;
    bool ok = true;
    std::vector<BlockCandidate> candidates;
    std::vector<FailedBlock> failures;
};

static AttemptResult evaluate_attempt(const WavData &original, double filter_ms) {
    WavData wav = original;
    if (filter_ms > 0.0) remove_slow_baseline(wav, filter_ms);

    BitWriter writer(nullptr, nullptr);
    decode(wav, writer);

    FILE *log = tmpfile();
    if (!log) die("cannot create temporary file for automatic DC-filter trial");

    AttemptResult r;
    r.filter_ms = filter_ms;
    r.bit_count = writer.bit_count();
    BlockProcessor processor(nullptr, log, r.stats, &r.candidates, filter_ms, &r.failures);
    r.ok = processor.process(writer.events());
    fclose(log);
    return r;
}

static bool intervals_overlap(const BlockCandidate &a, const BlockCandidate &b) {
    const uint32_t lo = std::max(a.start_sample, b.start_sample);
    const uint32_t hi = std::min(a.end_sample, b.end_sample);
    if (hi > lo) return true;

    // Start positions for the same physical block normally differ by only a few
    // samples between filter passes.  This tolerance also catches a candidate
    // whose end marker shifted just enough that the intervals no longer overlap.
    const uint32_t d = a.start_sample > b.start_sample
        ? a.start_sample - b.start_sample : b.start_sample - a.start_sample;
    return d <= 2000;
}

static bool candidate_preferred(const BlockCandidate &a, const BlockCandidate &b);

static bool failure_overlaps_candidate(const FailedBlock &f, const BlockCandidate &c) {
    const uint32_t lo = std::max(f.start_sample, c.start_sample);
    const uint32_t hi = std::min(f.end_sample, c.end_sample);
    if (hi > lo) return true;
    const uint32_t d = f.start_sample > c.start_sample
        ? f.start_sample - c.start_sample : c.start_sample - f.start_sample;
    return d <= 2000;
}

static const BlockCandidate *find_recovery_for_failure(
        const FailedBlock &f, const std::vector<BlockCandidate> &selected) {
    const BlockCandidate *best = nullptr;
    for (const BlockCandidate &c : selected) {
        if (!failure_overlaps_candidate(f, c)) continue;
        if (!best || candidate_preferred(c, *best)) best = &c;
    }
    return best;
}

static bool candidate_overlaps_any_failure(
        const BlockCandidate &c, const std::vector<FailedBlock> &failures) {
    for (const FailedBlock &f : failures) {
        if (failure_overlaps_candidate(f, c)) return true;
    }
    return false;
}

static bool candidate_preferred(const BlockCandidate &a, const BlockCandidate &b) {
    // A checksum-valid numeric block is stronger evidence than a non-checksummed
    // block.  All candidates reaching here already passed the normal write policy.
    if (a.numeric != b.numeric) return a.numeric;

    // For the same physical block, keep the unfiltered result whenever it was
    // already valid.  Alternate filters are recovery tools, not replacements.
    if ((a.filter_ms == 0.0) != (b.filter_ms == 0.0)) return a.filter_ms == 0.0;

    // Otherwise prefer the larger complete payload.  For equal payloads use
    // the trial order rather than simply choosing the numerically smallest
    // window: 1 ms has proven the most useful local-baseline correction, with
    // 0.5 ms as the next fallback.
    if (a.data.size() != b.data.size()) return a.data.size() > b.data.size();
    auto filter_rank = [](double ms) {
        const double order[] = {0.0, 1.0, 0.5, 2.0, 4.0, 8.0, 12.0, 20.0, 50.0};
        for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
            if (std::fabs(ms - order[i]) < 1.0e-9) return i;
        }
        return sizeof(order) / sizeof(order[0]);
    };
    return filter_rank(a.filter_ms) < filter_rank(b.filter_ms);
}

static std::vector<BlockCandidate> merge_block_candidates(
        const std::vector<AttemptResult> &attempts) {
    std::vector<BlockCandidate> selected;

    // Seed with the unfiltered pass.  It is the reference decoding and every
    // block accepted there remains untouched.
    if (!attempts.empty()) selected = attempts.front().candidates;

    // Later passes may only add a block where the current selection has no
    // overlapping valid block.  If two alternate passes fill the same gap, keep
    // the stronger candidate according to candidate_preferred().
    for (size_t ai = 1; ai < attempts.size(); ++ai) {
        for (const BlockCandidate &c : attempts[ai].candidates) {
            size_t overlap = selected.size();
            for (size_t i = 0; i < selected.size(); ++i) {
                if (intervals_overlap(c, selected[i])) {
                    overlap = i;
                    break;
                }
            }
            if (overlap == selected.size()) {
                selected.push_back(c);
            } else if (selected[overlap].filter_ms != 0.0 &&
                       candidate_preferred(c, selected[overlap])) {
                // Never replace an unfiltered valid block.  Only arbitrate
                // between alternate-filter candidates that fill the same gap.
                selected[overlap] = c;
            }
        }
    }

    std::sort(selected.begin(), selected.end(),
              [](const BlockCandidate &a, const BlockCandidate &b) {
                  if (a.start_sample != b.start_sample) return a.start_sample < b.start_sample;
                  return a.end_sample < b.end_sample;
              });
    return selected;
}

int main(int argc, char **argv) {
    Options opt = parse_args(argc, argv);
    const WavData original_wav = read_wav(opt.input);

    double selected_filter_ms = opt.dc_filter_ms;
    std::vector<AttemptResult> auto_attempts;
    std::vector<BlockCandidate> auto_selected;

    if (opt.dc_filter_auto) {
        // Block-level auto mode: the unfiltered pass is the backbone.  Alternate
        // filters are decoded as independent passes and may only fill gaps where
        // the backbone did not yield a valid block.
        const double candidates[] = {0.0, 1.0, 0.5, 2.0, 4.0, 8.0, 12.0, 20.0, 50.0};
        for (double f : candidates) {
            AttemptResult r = evaluate_attempt(original_wav, f);
            fprintf(stderr,
                    "Auto block trial: %s -> %llu valid block%s, %llu bytes, %llu framing-invalid, %llu content-invalid, %llu false-first-byte acquisitions\n",
                    f == 0.0 ? "unfiltered" : (std::to_string(f) + " ms").c_str(),
                    static_cast<unsigned long long>(r.stats.written), r.stats.written == 1 ? "" : "s",
                    static_cast<unsigned long long>(r.stats.bytes_written),
                    static_cast<unsigned long long>(r.stats.framing_bad),
                    static_cast<unsigned long long>(r.stats.content_bad),
                    static_cast<unsigned long long>(r.stats.false_first_byte_acquisitions));
            auto_attempts.push_back(std::move(r));
        }
        auto_selected = merge_block_candidates(auto_attempts);
        selected_filter_ms = 0.0; // diagnostic bits/trace remain unfiltered in auto mode
        fprintf(stderr, "Auto block merge selected %zu physical blocks\n", auto_selected.size());
    }

    WavData wav = original_wav;
    if (selected_filter_ms > 0.0) {
        remove_slow_baseline(wav, selected_filter_ms);
    }

    FILE *log_fp = stderr;
    if (!opt.log.empty()) {
        log_fp = fopen(opt.log.c_str(), "wb");
        if (!log_fp) { perror(opt.log.c_str()); return 1; }
    }

    fprintf(log_fp, "dpwav2tap-integrated-v11-score: knee/PLL decoder + configurable/block-level automatic local-baseline retry + multi-region checksum/pattern recovery\n");
    fprintf(log_fp, "Input: %s\n", opt.input);
    fprintf(log_fp, "WAV: %u Hz, %zu mono samples (%.2f s)\n",
            wav.sample_rate, wav.samples.size(),
            wav.samples.empty() ? 0.0 : static_cast<double>(wav.samples.size()) / wav.sample_rate);
    if (selected_filter_ms > 0.0) {
        fprintf(log_fp, "DC filter: %.3f ms centered moving-average subtraction\n", selected_filter_ms);
    } else if (opt.dc_filter_auto) {
        fprintf(log_fp, "DC filter auto: block-level merge; diagnostic bits/trace use unfiltered waveform\n");
    }

    FILE *bits_fp = nullptr;
    if (!opt.bits_output.empty()) {
        bits_fp = fopen(opt.bits_output.c_str(), "wb");
        if (!bits_fp) { perror(opt.bits_output.c_str()); if (log_fp != stderr) fclose(log_fp); return 1; }
    }

    FILE *trace_fp = nullptr;
    if (!opt.trace.empty()) {
        trace_fp = fopen(opt.trace.c_str(), "wb");
        if (!trace_fp) {
            perror(opt.trace.c_str());
            if (bits_fp) fclose(bits_fp);
            if (log_fp != stderr) fclose(log_fp);
            return 1;
        }
    }

    BitWriter writer(bits_fp, trace_fp);
    decode(wav, writer);

    if (bits_fp) fclose(bits_fp);
    if (trace_fp) fclose(trace_fp);

    FILE *tap_fp = fopen(opt.tap_output.c_str(), "wb");
    if (!tap_fp) {
        perror(opt.tap_output.c_str());
        if (log_fp != stderr) fclose(log_fp);
        return 1;
    }

    BlockStats stats;
    bool ok = true;
    if (opt.dc_filter_auto) {
        for (const BlockCandidate &c : auto_selected) {
            if (!write_tap_record(tap_fp, c.data)) {
                fprintf(log_fp, "TAP write FAILED while writing merged block at WAV sample %u.\n",
                        c.start_sample);
                ok = false;
                break;
            }
            ++stats.written;
            stats.bytes_written += c.data.size();
            fprintf(log_fp,
                    "Merged block: WAV samples %u..%u, %zu bytes, source %s%s. Written to TAP.\n",
                    c.start_sample, c.end_sample, c.data.size(),
                    c.filter_ms == 0.0 ? "unfiltered" : "dc-filter ",
                    c.filter_ms == 0.0 ? "" : (std::to_string(c.filter_ms) + " ms").c_str());
        }
        // Report aggregate failures from the unfiltered backbone only; alternate
        // pass failures are expected during recovery trials and are not final errors.
        if (!auto_attempts.empty()) {
            stats.synced_blocks = auto_attempts[0].stats.synced_blocks;
            stats.completed_blocks = auto_attempts[0].stats.completed_blocks;
            stats.empty_blocks = auto_attempts[0].stats.empty_blocks;
            stats.false_first_byte_acquisitions =
                auto_attempts[0].stats.false_first_byte_acquisitions;
            stats.framing_bad = auto_attempts[0].stats.framing_bad;
            stats.content_bad = auto_attempts[0].stats.content_bad;
        }
    } else {
        BlockProcessor processor(tap_fp, log_fp, stats);
        ok = processor.process(writer.events());
    }
    if (fclose(tap_fp) != 0) {
        perror(opt.tap_output.c_str());
        if (log_fp != stderr) fclose(log_fp);
        return 1;
    }

    if (opt.dc_filter_auto && !auto_attempts.empty()) {
        const BlockStats &backbone = auto_attempts[0].stats;
        const uint64_t auto_added = stats.written >= backbone.written
            ? stats.written - backbone.written : 0;

        const std::vector<FailedBlock> &known_failures = auto_attempts[0].failures;
        size_t recovered_failures = 0;
        size_t unresolved_failures = 0;
        fprintf(log_fp, "\nKnown-block recovery report:\n");
        for (const FailedBlock &f : known_failures) {
            const BlockCandidate *r = find_recovery_for_failure(f, auto_selected);
            const char *kind = f.kind == FailedBlock::Kind::CONTENT ? "content/checksum" : "framing";
            if (r && r->filter_ms != 0.0) {
                ++recovered_failures;
                fprintf(log_fp,
                        "  RECOVERED: unfiltered block %llu %s failure at WAV %u..%u "
                        "(%zu bytes decoded) -> %zu-byte valid block using %.3f ms filter.\n",
                        static_cast<unsigned long long>(f.block_number), kind,
                        f.start_sample, f.end_sample, f.decoded_bytes, r->data.size(), r->filter_ms);
            } else if (r) {
                // Normally impossible for a failure from the unfiltered pass, but
                // keep the accounting robust if candidate collection changes later.
                ++recovered_failures;
                fprintf(log_fp,
                        "  RECOVERED: unfiltered block %llu %s failure at WAV %u..%u "
                        "is covered by an accepted unfiltered block.\n",
                        static_cast<unsigned long long>(f.block_number), kind,
                        f.start_sample, f.end_sample);
            } else {
                ++unresolved_failures;
                fprintf(log_fp,
                        "  UNRESOLVED: unfiltered block %llu %s failure at WAV %u..%u "
                        "after %zu decoded byte%s; no valid block found by any filter.\n",
                        static_cast<unsigned long long>(f.block_number), kind,
                        f.start_sample, f.end_sample, f.decoded_bytes,
                        f.decoded_bytes == 1 ? "" : "s");
            }
        }

        size_t missed_then_found = 0;
        for (const BlockCandidate &c : auto_selected) {
            if (c.filter_ms == 0.0) continue;
            if (candidate_overlaps_any_failure(c, known_failures)) continue;
            ++missed_then_found;
            fprintf(log_fp,
                    "  RECOVERED MISSED BLOCK: WAV %u..%u, %zu bytes, found only with %.3f ms filter.\n",
                    c.start_sample, c.end_sample, c.data.size(), c.filter_ms);
        }

        const size_t plausible_blocks = auto_selected.size() + unresolved_failures;
        const double extraction_score = plausible_blocks == 0 ? 100.0 :
            100.0 * static_cast<double>(auto_selected.size()) / static_cast<double>(plausible_blocks);
        fprintf(log_fp,
                "Extraction score: %.2f / 100 (known-block coverage). "
                "%zu accepted physical blocks, %zu recovered known failure%s, "
                "%zu recovered block%s missed by unfiltered decoding, %zu unresolved plausible block%s.\n",
                extraction_score, auto_selected.size(), recovered_failures,
                recovered_failures == 1 ? "" : "s", missed_then_found,
                missed_then_found == 1 ? "" : "s", unresolved_failures,
                unresolved_failures == 1 ? "" : "s");
        if (unresolved_failures == 0) {
            fprintf(log_fp,
                    "Status: COMPLETE WITH RESPECT TO ALL KNOWN BLOCK FAILURES - no discarded/NOT WRITTEN "
                    "plausible block remains unrecovered.\n");
        } else {
            fprintf(log_fp,
                    "Status: ATTENTION NEEDED - %zu plausible block%s still failed in every decoding pass.\n",
                    unresolved_failures, unresolved_failures == 1 ? "" : "s");
        }
        fprintf(log_fp,
                "Note: a score of 100 means no *known* plausible block failure remains; it cannot prove "
                "that a block missed by every sync detector does not exist.\n\n");

        fprintf(log_fp,
                "Summary: %llu decoded bits (unfiltered diagnostic pass).\n"
                "Unfiltered backbone: %llu sync acquisitions, %llu completed blocks, "
                "%llu empty-block false acquisitions, %llu silent first-byte false acquisitions, "
                "%llu framing-invalid, %llu content-invalid, %llu valid blocks.\n"
                "Auto merge: %llu TAP records (%llu unfiltered + %llu recovered from alternate filters), "
                "%llu bytes written to %s.\n",
                static_cast<unsigned long long>(writer.bit_count()),
                static_cast<unsigned long long>(backbone.synced_blocks),
                static_cast<unsigned long long>(backbone.completed_blocks),
                static_cast<unsigned long long>(backbone.empty_blocks),
                static_cast<unsigned long long>(backbone.false_first_byte_acquisitions),
                static_cast<unsigned long long>(backbone.framing_bad),
                static_cast<unsigned long long>(backbone.content_bad),
                static_cast<unsigned long long>(backbone.written),
                static_cast<unsigned long long>(stats.written),
                static_cast<unsigned long long>(backbone.written),
                static_cast<unsigned long long>(auto_added),
                static_cast<unsigned long long>(stats.bytes_written),
                opt.tap_output.c_str());
    } else {
        fprintf(log_fp,
                "Summary: %llu decoded bits, %llu sync acquisitions, %llu completed blocks, "
                "%llu empty-block false acquisitions, %llu silent first-byte false acquisitions, "
                "%llu framing-invalid, %llu content-invalid, %llu TAP records, %llu bytes written to %s\n",
                static_cast<unsigned long long>(writer.bit_count()),
                static_cast<unsigned long long>(stats.synced_blocks),
                static_cast<unsigned long long>(stats.completed_blocks),
                static_cast<unsigned long long>(stats.empty_blocks),
                static_cast<unsigned long long>(stats.false_first_byte_acquisitions),
                static_cast<unsigned long long>(stats.framing_bad),
                static_cast<unsigned long long>(stats.content_bad),
                static_cast<unsigned long long>(stats.written),
                static_cast<unsigned long long>(stats.bytes_written),
                opt.tap_output.c_str());
    }
    if (!opt.bits_output.empty()) fprintf(log_fp, "Bits written to %s\n", opt.bits_output.c_str());
    if (!opt.trace.empty()) fprintf(log_fp, "Raw bit trace written to %s\n", opt.trace.c_str());

    if (log_fp != stderr) fclose(log_fp);
    return ok ? 0 : 1;
}
