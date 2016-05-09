import os
import json
import sys

EXPECTED_BEHAVIOR = ('OK', 'UNIMPLEMENTED', 'INFORMATIONAL')
EXPECTED_BEHAVIOR_CLOSE = ('OK', 'INFORMATIONAL')

args = sys.argv[1:]
fn = os.path.abspath(args[0])
indexPath = os.path.dirname(fn)
relativeToIndex = lambda f: os.path.join(indexPath, f)
print "index", fn

failures = []

with open(fn, 'r') as fh:
    index = json.load(fh)
    for servername, serverResults in index.items():
        for test in serverResults:
            result = serverResults[test]
            if ((result['behavior'] not in EXPECTED_BEHAVIOR) or
                 result['behaviorClose'] not in EXPECTED_BEHAVIOR_CLOSE):
                with open(relativeToIndex(result['reportfile'])) as rh:
                    report = json.load(rh)
                    failures.append(report)

if failures:
    print >> sys.stderr, json.dumps(failures, indent=2)
    print >> sys.stderr, 'there was %s failures' % len(failures)
    sys.exit(1)
