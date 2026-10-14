/* What console and what environment this run is on, for the log: the
 * firmware, and the homebrew processes running beside AnyPad PS5 (the ones
 * that are not the console's own, whose names start with "Sce"). With logs
 * from several consoles this shows which firmware and which payloads a result
 * belongs to. Read-only. */
#ifndef ANYPAD_PS5_SYSINFO_H
#define ANYPAD_PS5_SYSINFO_H

void sysinfo_log(void);

#endif
