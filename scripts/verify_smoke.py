"""Validate a fresh Copy Analyzer smoke run on either CI OS."""
from __future__ import annotations
import argparse,csv,json,math
from pathlib import Path

def require(cond: bool,msg: str)->None:
    if not cond: raise ValueError(msg)

def main()->None:
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('folder',type=Path);ap.add_argument('--platform',choices=('Linux','Windows'),required=True);args=ap.parse_args()
    reports=list(args.folder.glob('*/results.json'));require(len(reports)==1,'Expected exactly one fresh smoke session');report=reports[0];data=json.loads(report.read_text(encoding='utf-8'))
    require(data['schema']=='AM5Native/4','Unexpected report schema');require(data.get('focus')=='copy_bottleneck','Wrong analysis focus');require(data['engine']=='AM5-Native-2.2.0-COPY-AVX2','Unexpected engine id');require(data['platform'].startswith(args.platform),'Wrong executable platform');require(data['complete'] is True,'Benchmark did not complete');require(data['selftests_passed']==60,'Not all 60 self-tests passed');require(data['integrity']['errors']==0,'Data verification failed');require(data['options']['smoke'] is True,'Not a smoke run');require(data['sufficient_memory'] is False,'Smoke results must not be labelled DRAM scores');require(data['profile_loaded'] is True,'Packaged profile could not be read');require(data['machine']['avx2'] is True,'AVX2 support missing');require(data['timer_frequency_hz']>0,'Invalid timer frequency');require(data['diagnostics']==[],'Smoke mode must suppress performance diagnosis')
    samples=data['samples'];require(samples,'No samples recorded')
    for s in samples:
        label=f"{s['suite']}/{s['test']}";require(s['errors']==0,f'Sample integrity error: {label}');require(s['pin_ok'] is True,f'Thread affinity failed: {label}');require(math.isfinite(s['elapsed']) and s['elapsed']>0,f'Invalid elapsed: {label}');require(math.isfinite(s['value']) and s['value']>0,f'Invalid value: {label}')
        if s['unit']=='GB/s': expected=s['logical_bytes']/s['elapsed']/1e9
        else:
            require(s['unit']=='ns/access',f'Unknown unit: {label}');require(s['operations']>0,f'No operations: {label}');expected=s['elapsed']*1e9/s['operations'];require(0<s['p50']<=s['p95']<=s['p99']<=s['p999']<=s['max'],f'Bad quantiles: {label}')
        require(math.isclose(s['value'],expected,rel_tol=1e-9),f'Timing/byte arithmetic mismatch: {label}')
    pats=sorted({s['pattern_bytes'] for s in samples if s['suite']=='copy_turnaround'});require(pats==[64,128,256,512,1024,2048,4096,8192,16384,65536],f'Wrong turnaround sweep: {pats}')
    require(any(s['suite']=='copy_baseline' and s['test']=='copy_nt' for s in samples),'Missing Copy baseline');require(any(s['suite']=='loaded_latency' and s['test']=='copy_load' for s in samples),'Missing Copy loaded-latency probe');require(any(s['suite']=='copy_stall_probe' and s['test']=='idle_short_window' for s in samples),'Missing idle short-window probe');require(any(s['suite']=='copy_stall_probe' and s['test']=='copy_load_short_window' for s in samples),'Missing Copy-loaded short-window probe')
    with report.with_name('raw.csv').open(encoding='utf-8',newline='') as h: rows=list(csv.DictReader(h))
    require(rows,'CSV is empty');require({'pattern_bytes','events','batch_p999_ns','batch_max_ns'}.issubset(rows[0]),'New CSV fields missing');require(len(rows)==len(samples),'CSV/JSON sample counts differ')
    for row,s in zip(rows,samples): require((row['suite'],row['test'],int(row['trial']))==(s['suite'],s['test'],s['trial']),'CSV/JSON identity mismatch')
    html=report.with_name('report.html').read_text(encoding='utf-8');require('__DATA__' not in html and 'AM5Native/4' in html and 'Copy 短板排名' in html,'HTML report did not embed v4 data')
    print(f"PASS: {args.platform}; 60 self-tests; {len(samples)} smoke samples; Copy v4 JSON/CSV/HTML verified.")
    print('Functional CI checks only: not a performance reference or overclock stability certificate.')

if __name__=='__main__': main()
