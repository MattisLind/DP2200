"""Regression check for long command lines in a real curses pseudo-terminal."""
import os,pty,termios,fcntl,struct,subprocess,select,time,tempfile,signal
from pathlib import Path
sim=Path(__file__).resolve().parents[1]/'dp2200sim'
with tempfile.TemporaryDirectory() as work:
 master,slave=pty.openpty()
 fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,200,0,0))
 env=os.environ.copy();env['TERM']='xterm'
 env.setdefault('SDL_VIDEODRIVER','dummy')
 env.setdefault('SDL_RENDER_DRIVER','software')
 def reset_signals():signal.pthread_sigmask(signal.SIG_SETMASK,[])
 proc=subprocess.Popen([str(sim)],cwd=work,stdin=slave,stdout=slave,stderr=slave,env=env,preexec_fn=reset_signals)
 os.close(slave);output=bytearray()
 def pump(seconds):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if select.select([master],[],[],min(.05,max(0,end-time.monotonic())))[0]:
    try:output.extend(os.read(master,65536))
    except OSError:break
 def send(data):os.write(master,data);pump(.3)
 try:
  pump(.4)
  command=b'ATTACH TYPE=9374 DRIVE=0 FILE=dos-d-results/dos-d-2.6/removable.dsk WRITEPROTECT=FALSE'
  # Isolated image; same long path as the reported crash.
  Path(work,'dos-d-results/dos-d-2.6').mkdir(parents=True)
  send(command)
  send(b'\x01X\x7f') # Home, insert, backspace back to original text.
  send(b'\x05X\x1bOD\x1b[3~') # End, insert, left, delete.
  send(b'\n')
  image=Path(work,'dos-d-results/dos-d-2.6/removable.dsk')
  deadline=time.monotonic()+5
  while not image.exists() and proc.poll() is None and time.monotonic()<deadline:pump(.1)
  assert image.exists(), Path(work,'dp2200.log').read_text()[-6000:]
  assert image.stat().st_size==10027008
  send(b'\x1bOA\n') # Recall and submit the same long command.
  assert proc.poll() is None
  send(b'\x1bOA\x1bOB') # Recall then return to empty input.
  send(b'QUIT\n');pump(.3)
  assert proc.wait(timeout=2)==0
  assert b'out_of_range' not in output
  print('PASS: long ATTACH, Ctrl-A/E, insertion, backspace, arrows, Delete, history and QUIT; correct image attached')
 finally:
  if proc.poll() is None:proc.kill();proc.wait(timeout=2)
  os.close(master)
