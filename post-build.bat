:: Remember to update your mod's directory!!
set MY_AI_OUTPUT="E:\Program Files (x86)\Steam\steamapps\common\Reassembly\mods\gungines-test\ai\"
::echo %MY_AI_OUTPUT%
::pause

:: Copy AI DLL and PDB (for debugging) to your mod's dir
copy /Y Release\*.dll %MY_AI_OUTPUT%
copy /Y Release\*.pdb %MY_AI_OUTPUT%
pause