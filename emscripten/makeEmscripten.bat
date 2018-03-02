::  POOR MAN'S DOS PROMPT BUILD SCRIPT.. make sure to delete the respective built/*.bc files before building!
::  existing *.bc files will not be recompiled. 

setlocal enabledelayedexpansion

SET ERRORLEVEL
VERIFY > NUL


::-DBUILD_FOR_SIZE -DBUILD_FOR_SPEED 

:: **** use the "-s WASM" switch to compile WebAssembly output. warning: the SINGLE_FILE approach does NOT currently work in Chrome 63.. ****
set "OPT= -s WASM=0 -s ASSERTIONS=1 -s VERBOSE=0 -s FORCE_FILESYSTEM=1  -DNO_DEBUG_LOGS -DHAVE_LIMITS_H -DHAVE_STDINT_H -Wcast-align -fno-strict-aliasing -s SAFE_HEAP=0 -s DISABLE_EXCEPTION_CATCHING=0 -Wno-pointer-sign -I. -I.. -I../src -I../src/common -I../src/nes -I../src/sdlplay -I../zlib -Os -O3 "
if not exist "built/zlib.bc" (
	call emcc.bat %OPT% ../zlib/adler32.c ../zlib/compress.c ../zlib/crc32.c ../zlib/gzio.c ../zlib/uncompr.c ../zlib/deflate.c ../zlib/trees.c ../zlib/zutil.c ../zlib/inflate.c ../zlib/infback.c ../zlib/inftrees.c ../zlib/inffast.c  -o built/zlib.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
if not exist "built/nez.bc" (
	call emcc.bat   %OPT% ../src/common/zlib/nez.c ../src/common/zlib/memzip.c -o built/nez.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
if not exist "built/cpu.bc" (
	call emcc.bat  %OPT% ../src/nes/kmz80/kmz80t.c ../src/nes/kmz80/kmz80c.c ../src/nes/kmz80/kmz80.c ../src/nes/kmz80/kmevent.c ../src/nes/kmz80/kmdmg.c -o built/cpu.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
::../src/nes/device/s_fdse.c 
if not exist "built/device.bc" (
	call emcc.bat   %OPT% ../src/nes/device/ym2151/ym2151.c ../src/nes/device/s_opm.c ../src/nes/device/s_vrc7.c ../src/nes/device/s_vrc6.c ../src/nes/device/s_sng.c ../src/nes/device/s_scc.c ../src/nes/device/s_psg.c ../src/nes/device/s_opltbl.c ../src/nes/device/s_opl.c ../src/nes/device/s_n106.c ../src/nes/device/s_mmc5.c ../src/nes/device/s_logtbl.c ../src/nes/device/s_hes.c ../src/nes/device/s_fme7.c ../src/nes/device/s_fds3.c ../src/nes/device/s_fds2.c ../src/nes/device/s_fds1.c ../src/nes/device/s_fds.c ../src/nes/device/s_dmg.c ../src/nes/device/s_deltat.c ../src/nes/device/s_apu.c ../src/nes/device/logtable.c -o built/device.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
if not exist "built/nes.bc" (
	call emcc.bat   %OPT% ../src/nes/songinfo.c ../src/nes/nsf6502.c ../src/nes/nsdplay.c ../src/nes/nsdout.c ../src/nes/m_sgc.c ../src/nes/m_zxay.c ../src/nes/m_nsf.c ../src/nes/m_kss.c ../src/nes/m_hes.c ../src/nes/m_gbr.c ../src/nes/audiosys.c ../src/nes/handler.c -o built/nes.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
call emcc.bat %OPT% -s TOTAL_MEMORY=67108864 --closure 1 --llvm-lto 1  --memory-init-file 0 built/zlib.bc built/nez.bc built/cpu.bc built/device.bc built/nes.bc  ../src/sdlplay/nlg.c adapter.cpp   -s EXPORTED_FUNCTIONS="[ '_emu_load_file','_emu_teardown','_emu_get_current_position','_emu_seek_position','_emu_get_max_position','_emu_set_subsong','_emu_get_track_info','_emu_get_sample_rate','_emu_get_audio_buffer','_emu_get_audio_buffer_length','_emu_compute_audio_samples', '_malloc', '_free']"  -o htdocs/nez.js  -s SINGLE_FILE=0 -s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'Pointer_stringify']"  -s BINARYEN_ASYNC_COMPILATION=1 -s BINARYEN_TRAP_MODE='clamp' && copy /b shell-pre.js + htdocs\nez.js + shell-post.js htdocs\web_nez3.js && del htdocs\nez.js && copy /b htdocs\web_nez3.js + nez_adapter.js htdocs\backend_nez.js && del htdocs\web_nez3.js
:END

