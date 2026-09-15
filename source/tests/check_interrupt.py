from pathlib import Path
import subprocess,time,signal,json
P=Path(__file__).resolve().parents[1]
root=P/'tests/interrupted';root.mkdir(exist_ok=True)
log=(P/'tests/interrupt.log').open('w')
p=subprocess.Popen([str(P/'build/am5lab-linux'),'--no-open','--threads','2','--out',str(root),'--config',str(P/'profile.ini')],stdout=log,stderr=subprocess.STDOUT)
time.sleep(2);p.send_signal(signal.SIGINT)
rc=p.wait(timeout=15);log.close()
assert rc==2,rc
f=sorted(root.glob('*/results.json'))[-1]
d=json.loads(f.read_text());assert not d['complete'];assert isinstance(d['samples'],list)
print(f'Interrupt passed: exit={rc}, complete=false, saved_samples={len(d["samples"])}')
(P/'tests/interrupt-validation.txt').write_text(f'PASS SIGINT exits without worker deadlock\nPASS exit code 2\nPASS complete=false\nPASS valid partial JSON with {len(d["samples"])} completed samples\n')
