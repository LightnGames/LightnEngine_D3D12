@echo off

for %%f in (%*) do (
  "%~dp0\texconv" -f BC7_UNORM_SRGB %%f -m 0 -y
)
pause