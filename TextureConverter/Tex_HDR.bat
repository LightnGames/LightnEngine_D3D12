@echo off

for %%f in (%*) do (
  "%~dp0\texconv" -f BC6H_UF16 %%f -m 0 -y
)
pause