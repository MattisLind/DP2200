# Testing the presentation

The editable source is `presentation.qmd`. It uses Quarto + Reveal.js with the built-in dark theme and local styling in `presentation.css`. The numbered slide folders remain available as source material; they are not automatically included in this deck.

Run these commands from the repository root.

## Live preview

```sh
build/tools/quarto/bin/quarto preview presentation/presentation.qmd --host 127.0.0.1 --port 4200 --no-browser
```

Open http://localhost:4200/. Save changes to the QMD or CSS to refresh the preview. Stop the server with Ctrl+C. If an existing preview already uses port 4200, use it or choose a different port.

## Render a portable HTML file

```sh
build/tools/quarto/bin/quarto render presentation/presentation.qmd
```

Open `presentation/presentation.html` in a browser. Render mode embeds the images, styles, and presentation resources into the HTML. Preview mode can use separate resources while serving the deck; render again before copying the HTML elsewhere.

Quarto 1.11.9 is installed locally under `build/tools/quarto`, without a system-wide installation. If you later install Quarto normally, replace `build/tools/quarto/bin/quarto` with `quarto`.

## Presenting and editing

- Arrow keys or Space: advance through slides.
- F: full screen. Esc: slide overview.
- S: speaker view (best used through the local preview server; allow its popup).
- For picture-only slides, separate slides with `---` on its own line. A `## Heading` can also start a slide when a visible title is wanted.
- Put speaker notes inside a `::: {.notes}` block, closed with `:::`.
- Image paths are relative to `presentation.qmd`, such as `Bilder/Datapoint2200.jpg`.

The presentation uses a 1280 × 720 dark layout, with slide numbers and automatic image stretching disabled. The museum photographs are stacked vertically. Layout helpers are described below.

Quarto documentation: https://quarto.org/docs/presentations/revealjs/

## Print speaker notes only

After editing `.notes` blocks in `presentation.qmd`, run from the repository root:

```sh
python3 presentation/scripts/export_notes.py
```

Open `presentation/speaker-notes.html` in a browser and print (Cmd+P), or choose Save as PDF. This is an A4 document with slide numbers and image filenames as reminders, without the slide pictures. Slide numbering counts the automatic title slide as slide 1. Notes for each slide stay together where possible. The generated `speaker-notes.qmd` and HTML are replaced on each export; edit your notes in `presentation.qmd`.

## Click to advance

Click anywhere in the middle 70% of the screen (including photographs) to advance. Links, media controls, text selection, and Reveal controls keep their normal behavior. This is disabled in overview, paused, speaker-notes, and PDF print views. The handler is in `click-to-advance.html`.

## Picture layouts and captions

A single photograph:

```markdown
![](Bilder/Datapoint2200.jpg){.slide-photo}
```

The `slide-photo` class gives the image a 680px-high frame and preserves its proportions without cropping. Add `.with-caption` to reduce the frame to 600px and leave room for text:

```markdown
![](Bilder/datapoint-2200-product-introduction.png){.slide-photo .with-caption}

::: {style="text-align: center; font-size: 0.45em;"}
Photo: Photographer / Source
:::
```

Use an explicit text block for attributions: ordinary figure captions are hidden by `presentation.css`. In a multi-column slide, place each attribution inside its image's column. A rotated frame is 680px high and must also be shortened if adding a caption underneath it.

For two vertically stacked pictures:

```markdown
::: {.photo-stack .centered-photo-stack}
![](Bilder/DP2200_sheet2.jpg){.museum-photo}

![](Bilder/2200CPU_Sheet3.jpg){.museum-photo}
:::
```

`photo-stack` gives both pictures the same width, preserves their individual proportions, and adds a 16px gap. `centered-photo-stack` centers the pair vertically within a 720px frame. The current 940px width fits the museum panoramas and schematic pair; taller images may require a narrower stack to fit.

The architecture comparison uses `.columns .architecture-comparison` for equal columns with a 24px gap. Its CPU board image is wrapped in `.rotated-photo-frame` and uses `.rotate-clockwise` for a 90° rotation. The rotation is sized to its containing frame, so it also fits inside a column. Keep nested fenced divs unambiguous: use more colons for outer containers than inner containers, as in the existing comparison slide.

## Title line breaks and speaker notes

The title uses a YAML block with Markdown hard line breaks:

```yaml
title: |
  Datapoint 2200\
  From IKEA kit to running\
  Was it the first PC?
```

Put private notes on the relevant slide, before its next separator:

```markdown
::: {.notes}
Explain the photograph and the main point of this slide.
:::

---
```

These notes appear in speaker view and the separate printable notes export, not on the projected slide. Regenerate the notes export after editing; it does not update automatically with the slide preview.

## Waveform image variants

The waveform assets are in `Bilder/`:

- `Tape-waveform.png`: original screenshot.
- `Tape-waveform-annotated-clean.png`: image-tool-edited version with the cursor and bottom explanatory sentence removed.
- `Tape-waveform-annotated-precise.png`: deterministic reconstruction from the original PNG, with only the cursor column repaired before drawing annotations. No JPEG conversion or image generation is used.
- `Tape-waveform-annotated-precise.svg`: the same reconstruction with vector labels, shading, and brackets over an embedded PNG.

Choose a variant by changing the image path in `presentation.qmd`, then save and preview or render. Existing variants are retained for comparison. See [annotation details](Bilder/Tape-waveform-annotations.md) for measurements and framing assumptions.

To regenerate the precise variant from the repository root:

```sh
PYTHONPATH=build/tools/python python3 presentation/scripts/annotate_waveform.py --remove-cursor --output Tape-waveform-annotated-precise
```

This script requires NumPy and PyMuPDF, currently installed under `build/tools/python`. It writes the PNG, SVG, and JSON measurement report with the chosen output basename; it does not switch the deck's image reference.
