from pathlib import Path
import subprocess, json
P=Path(__file__).resolve().parent
B=P/'build';B.mkdir(exist_ok=True)
# Regenerate the embedded offline report before compiling either target.
template=(P/'report/template.html').read_text(encoding='utf-8')
h='#ifndef REPORT_TEMPLATE_H\n#define REPORT_TEMPLATE_H\nstatic const char REPORT_TEMPLATE[] =\n'
for line in template.splitlines(keepends=True):
 for i in range(0,len(line),1000): h+=json.dumps(line[i:i+1000],ensure_ascii=False)+'\n'
h+=';\n#endif\n'
(P/'src/report_template.h').write_text(h,encoding='utf-8')
src=['main','platform','util','kernels','bench','diagnostic','selftest','report']
def run(args):
 r=subprocess.run(args,cwd=P,text=True,capture_output=True)
 if r.stdout: print(r.stdout)
 if r.stderr: print(r.stderr)
 if r.returncode: raise RuntimeError('Failed: '+' '.join(args))
common_warnings=['-Wall','-Wextra','-Wpedantic','-Werror','-Wno-overlength-strings']
run(['clang','-std=c11','-O3',*common_warnings,'-pthread',*[f'src/{s}.c' for s in src],'-lm','-o',str(B/'am5lab-linux')])
mods={
 'kernel32':['ExitProcess','QueryPerformanceCounter','QueryPerformanceFrequency','VirtualAlloc','VirtualFree','CreateThread','WaitForSingleObject','CloseHandle','Sleep','SwitchToThread','GetCurrentThread','GetCurrentProcess','GetCurrentProcessId','SetThreadGroupAffinity','GetLogicalProcessorInformationEx','GlobalMemoryStatusEx','SetConsoleOutputCP','SetConsoleCtrlHandler','GetModuleFileNameW','WideCharToMultiByte','MultiByteToWideChar','GetCommandLineW','LocalFree','CreateDirectoryW','GetLastError','GetSystemTime','GetLocalTime','GetSystemTimes'],
 'shell32':['ShellExecuteW','CommandLineToArgvW'],
 'wevtapi':['EvtQuery','EvtNext','EvtRender','EvtClose'],
 'msvcrt':['malloc','calloc','realloc','free','qsort','strtoul','strtod','atoi','exit','printf','fprintf','fflush','fclose','fread','fwrite','fopen','_wfopen','_vscprintf','_vsnprintf','remove','memcpy','memmove','memset','memcmp','strlen','strcmp','strncmp','strchr','strrchr','strstr','sqrt','_aligned_malloc','_aligned_free']
}
for name,syms in mods.items():
 d=B/(name+'.def'); d.write_text('LIBRARY '+name.upper()+'.dll\nEXPORTS\n'+'\n'.join(syms)+'\n')
 run(['lld-link','/lib','/def:'+str(d),'/machine:x64','/out:'+str(B/(name+'.lib'))])
for s in src:
 # Clang's MSVC-target intrinsic headers expose AVX2 only when enabled
 # on the translation unit. Keep startup and CPU detection baseline x64.
 target_flags=['-mavx2'] if s=='kernels' else []
 run(['clang','--target=x86_64-pc-windows-msvc',*target_flags,'-std=c11','-ffreestanding','-fno-stack-protector','-O3',*common_warnings,'-Iwinshim','-c',f'src/{s}.c','-o',str(B/(s+'.obj'))])
run(['clang','--target=x86_64-pc-windows-msvc','-c','src/chkstk.S','-o',str(B/'chkstk.obj')])
run(['lld-link','/entry:mainCRTStartup','/subsystem:console,6.0','/nodefaultlib','/machine:x64','/dynamicbase','/nxcompat','/highentropyva','/stack:2097152','/opt:ref','/opt:icf','/timestamp:0','/out:'+str(B/'AM5MemoryLab.exe'),*[str(B/(s+'.obj')) for s in src],str(B/'chkstk.obj'),*[str(B/(s+'.lib')) for s in mods]])
print('Linux and Windows x64 builds complete.')
