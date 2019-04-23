@echo off

for %%f in (%*) do (
  "%~dp0\FBXConverter/x64/Release/FBXConverter" %%f
)
pause