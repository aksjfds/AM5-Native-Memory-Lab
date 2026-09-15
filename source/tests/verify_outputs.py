from pathlib import Path
import csv, json, math, statistics, subprocess, struct
P=Path(__file__).resolve().parents[1]
checks=[]
def check(v,s):
 if not v: raise AssertionError(s)
 checks.append(s)
normal=sorted((P/'tests/normal').glob('*/results.json'))[-1]
d=json.loads(normal.read_text())
check(d['complete'],'Normal benchmark completed')
check(d['selftests_passed']==56,'56 native self-tests recorded')
check(d['integrity']['errors']==0,'No observed data inconsistencies')
check(d['sufficient_memory'],'Normal main-memory working set exceeds cache guard')
check(d['options']['memory_bytes']==256*1048576,'Default array is 256 MiB on this validation machine')
check(len(d['samples'])==180,'Full 5-core validation suite yielded all 180 expected samples')
for s in d['samples']:
 check(s['value']>0 and math.isfinite(s['value']),'Finite positive measurement: '+s['suite']+'/'+s['test'])
 expected=s['logical_bytes']/s['elapsed']/1e9 if s['unit']=='GB/s' else s['elapsed']*1e9/s['operations']
 check(math.isclose(s['value'],expected,rel_tol=1e-10),'Independent byte/time or load/time recomputation')
 if s['unit']=='ns/access':
  check(s['p50']<=s['p95']<=s['p99'],'Latency-window quantiles are ordered')
  check(s['operations']%8192==0,'Pointer counts consist of 8192-load batches')
check(all(s['pin_ok'] for s in d['samples']),'All developer benchmark affinity calls succeeded')
rows=list(csv.DictReader(normal.with_name('raw.csv').open()))
check(len(rows)==len(d['samples']),'CSV/JSON sample counts match')
for x,y in zip(rows,d['samples']):
 check(x['suite']==y['suite'] and x['test']==y['test'] and int(x['trial'])==y['trial'],'CSV/JSON sample identity match')
 check(math.isclose(float(x['value']),y['value'],rel_tol=1e-8,abs_tol=1e-8),'CSV/JSON numerical values agree')
smoke=json.loads(sorted((P/'tests/smoke').glob('*/results.json'))[-1].read_text())
check(smoke['options']['smoke'] and not smoke['sufficient_memory'],'Smoke run flagged as non-DRAM-valid')
exe=P/'build/am5lab-linux'
for args in [['--threads','0'],['--memory-mib','-1'],['--sample-ms','x'],['--out'],['--unknown']]:
 r=subprocess.run([str(exe),*args],capture_output=True,text=True)
 check(r.returncode!=0,'Reject bad CLI: '+' '.join(args))
check(subprocess.run([str(exe),'--help'],capture_output=True).returncode==0,'Help is usable')
check(subprocess.run([str(exe),'--version'],capture_output=True).stdout.strip()==b'AM5-Native-2.0.0-AVX2','Version is stable')
pe=(P/'build/AM5MemoryLab.exe').read_bytes();check(pe[:2]==b'MZ','Windows binary has DOS header');pos=struct.unpack_from('<I',pe,0x3c)[0]
check(pe[pos:pos+4]==b'PE\0\0','Windows PE signature');check(struct.unpack_from('<H',pe,pos+4)[0]==0x8664,'Windows AMD64 machine type');check(struct.unpack_from('<H',pe,pos+24)[0]==0x20b,'PE32+ optional header')
# Certificate directory is zero: unsigned build is explicitly disclosed.
cert=struct.unpack_from('<II',pe,pos+24+112+8*4)
check(cert==(0,0),'Unsigned status independently verified')
audit=subprocess.check_output(['llvm-objdump','-p',str(P/'build/AM5MemoryLab.exe')],text=True)
imports=[x.split(':',1)[1].strip() for x in audit.splitlines() if 'DLL Name:' in x]
check(set(imports)=={'KERNEL32.dll','SHELL32.dll','WEVTAPI.dll','MSVCRT.dll'},'Only in-box Windows DLL imports; no redistributable DLL dependency')
(P/'tests/pe-inspection.txt').write_text(audit)
(P/'tests/numerical-validation.txt').write_text('\n'.join('PASS '+s for s in checks)+f'\n\n{len(checks)} assertions passed.\n')
print(f'{len(checks)} numerical/structural/CLI assertions passed. All numeric benchmark data are from the developer Linux environment, not the user PC.')
