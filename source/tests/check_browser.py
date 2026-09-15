from pathlib import Path
import json, statistics
from playwright.sync_api import sync_playwright
P=Path(__file__).resolve().parents[1]
normal=sorted((P/'tests/normal').glob('*/report.html'))[-1]
smoke=sorted((P/'tests/smoke').glob('*/report.html'))[-1]
checks=[]
with sync_playwright() as pw:
 b=pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
 page=b.new_page(viewport={'width':1440,'height':1100},device_scale_factor=1)
 errors=[];requests=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.on('request',lambda r:requests.append(r.url))
 page.set_content(normal.read_text(), wait_until="load");page.wait_for_function('window.LAB_REPORT !== undefined')
 assert not errors,errors;checks.append('Full report JavaScript executes without errors')
 r=page.evaluate('window.LAB_REPORT')
 assert r['valid'];checks.append('Complete normal run enables only the supported active diagnostic section')
 assert len(r['data']['samples'])==180;checks.append('All actual samples rendered')
 assert page.locator('#diagnosis .item').count()==6;checks.append('Six evidence-based active diagnostic views rendered')
 assert page.locator('#bandwidth tr').count()==13;checks.append('All throughput / ratio / group tests are visible')
 assert page.locator('#cache tr').count()==3;checks.append('All three cache-size rows rendered')
 assert page.locator('#candidates tr').count()==5;checks.append('Per-parameter attribution limitations are explicit')
 assert not any(u.startswith(('http://','https://')) for u in requests);checks.append('Report makes no network requests')
 groups=r['groups']
 expected=max(g['median'] for g in groups if g['suite']=='memory_bandwidth' and g['test']=='read')
 assert r['summary']['read']==expected;checks.append('Read summary equals observed best median, not a theoretical score')
 cp=next(g for g in groups if g['suite']=='memory_bandwidth' and g['test']=='copy_nt')
 assert r['summary']['copy_payload']==cp['median']/2;checks.append('Copy payload rate is half the explicit read+write logical rate')
 page.screenshot(path=str(P/'tests/report-desktop.png'),full_page=True)
 page.locator('summary').click()
 with page.expect_download() as dlinfo: page.locator('#download').click()
 dl=dlinfo.value;dl.save_as(str(P/'tests/browser-export.json'))
 exported=json.loads((P/'tests/browser-export.json').read_text());assert len(exported['samples'])==180;checks.append('Embedded JSON export works offline')
 page.set_viewport_size({'width':390,'height':844});page.screenshot(path=str(P/'tests/report-mobile.png'),full_page=False)
 assert page.evaluate('document.documentElement.scrollWidth <= innerWidth + 1');checks.append('Narrow layout has no whole-page horizontal overflow')
 page=b.new_page();page.on("pageerror",lambda e:errors.append(str(e)))
 page.set_content(smoke.read_text(), wait_until="load");page.wait_for_function('window.LAB_REPORT !== undefined');assert not page.evaluate('window.LAB_REPORT.valid');checks.append('Developer smoke data cannot produce normal DRAM diagnostic conclusions')
 assert not errors,errors;checks.append('No browser errors after all interactions')
 b.close()
(P/'tests/browser-validation.txt').write_text('\n'.join('PASS '+s for s in checks)+f'\n\n{len(checks)} browser assertions passed.\n')
print(f'{len(checks)} browser assertions passed.')
