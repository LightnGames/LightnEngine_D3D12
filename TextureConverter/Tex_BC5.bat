@echo off

for %%f in (%*) do (
  "%~dp0\texconv" -f BC5_UNORM %%f -m 0 -y
)
pause