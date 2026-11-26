Legend of Kairi (Sora in BotW) for Linux - third-party licenses
===============================================================

No game files are included: Savage Wind sets BotW up from your own dump, and the mod's data is built from your own KH2
files (by the Windows installer of the same version).

bin/savagewind and bin/sora_mod.so are built from Savage Wind (MIT, SavageWind-MIT.txt) plus this mod (the FidelityFX
FSR code built into savagewind: FidelityFX-LICENSE.txt). The other files in bin/:

File(s)                                              Library / file               License
---------------------------------------------------  ---------------------------  -------------------------------------------
deps/lib/libSDL3.so.0                                SDL 3                        zlib license (SDL3-zlib.txt)
deps/lib/libavcodec.so.63 libavformat.so.63          FFmpeg libraries (LGPL       LGPL-2.1-or-later (LGPL-2.1.txt)
  libavutil.so.61 libswscale.so.10                     build, no GPL parts)
deps/lib/libshaderc_shared.so.1                      Shaderc 2026.3 (Google)      Apache-2.0 (Apache-2.0.txt)
FiraSans-Medium.ttf FiraSans-SemiBold.ttf            Fira Sans                    SIL Open Font License 1.1 (FiraSans-OFL.txt)
NotoSansThai-Medium.ttf                              Noto Sans Thai               SIL Open Font License 1.1 (NotoSansThai-OFL.txt)
NotoSansHebrew-Medium.ttf                            Noto Sans Hebrew             SIL Open Font License 1.1 (NotoSansHebrew-OFL.txt)
colour-luts/ vk_pipelines.seed                       from the stock Savage Wind   MIT (SavageWind-MIT.txt)
                                                     0.6.0-alpha.6 AppImage

install.sh downloads the stock Savage Wind 0.6.0-alpha.6 AppImage (MIT, https://github.com/Revivedx/SavageWindRecomp) and
runs its own Set up; it is not shipped here.

Source code: https://ffmpeg.org/download.html , https://github.com/libsdl-org/SDL , https://github.com/google/shaderc

Credits: tvllerr (the mod); a lot of ideas came from PHH by YG123, a Borderlands 2 mod where you play as Sora from KH2 in a
similar way; the Savage Wind team; OpenKH; vgmstream; FFmpeg; SDL; Shaderc; Fira Sans; Noto Sans.
