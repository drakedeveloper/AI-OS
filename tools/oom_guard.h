#ifndef OOM_GUARD_H
#define OOM_GUARD_H

int protect_process(int pid);
int unprotect_process(int pid);

#endif