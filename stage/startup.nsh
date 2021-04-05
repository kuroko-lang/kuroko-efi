@echo -off
if exist fs0:\efi\boot\bootx64.efi then
  fs0:
  goto RUN
endif
if exist fs1:\efi\boot\bootx64.efi then
  fs1:
  goto RUN
endif
:RUN
\efi\boot\bootx64.efi
