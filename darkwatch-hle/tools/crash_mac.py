#!/usr/bin/env python3
# crash_mac.py: the newest macOS crash report of the harness -- exception, faulting stack, guest-relevant registers
import glob, json, os, sys
f = max(glob.glob(os.path.expanduser('~/Library/Logs/DiagnosticReports/darkwatch*.ips')), key=os.path.getmtime)
hdr, body = open(f).read().split('\n', 1)
d = json.loads(body)
print(os.path.basename(f), d['exception'].get('subtype') or d['exception'])
th = d['threads'][d['faultingThread']]
for fr in th['frames'][:int(sys.argv[1]) if len(sys.argv) > 1 else 14]:
    print(' ', fr.get('symbol', '?'), '+', fr.get('symbolLocation', ''))
