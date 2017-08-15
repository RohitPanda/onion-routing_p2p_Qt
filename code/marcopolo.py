import os
import sys
import subprocess

def conf(file):
	return os.path.join(file)

args = [
	'--mock-auth --mock-peer 127.0.0.1:10002 --mock-peer 127.0.0.1:10003 --polo --marco 127.0.0.1:10004 -c ' + conf('marcopolo1.conf'),
	'--mock-auth --mock-peer 127.0.0.1:10001 --polo -c ' + conf('marcopolo2.conf'),
	'--mock-auth --mock-peer 127.0.0.1:10001 --polo -c ' + conf('marcopolo3.conf'),
	'--mock-auth --mock-peer 127.0.0.1:10001 --polo -c ' + conf('marcopolo4.conf')
]

def find_onion(dir):
	o1 = os.path.join(dir, 'onion')
	if os.path.isfile(o1):
		return o1
	o2 = os.path.join(dir, 'onion.exe')
	if os.path.isfile(o2):
		return o2
	if dir == '.':
		return ''
	else:
		return find_onion('.')

# get folder from argv
if len(sys.argv) > 1:
	onion_dir = sys.argv[1]
else:
	onion_dir = '.'

onion = find_onion(onion_dir)
if onion == '':
	print('onion executable not found in "%s". Usage: python marcopolo.py <builddir>' % (onion_dir,))
	sys.exit(4)

procs = []
try:
	for idx, arg_raw in enumerate(args):
		arg_final = [onion] + arg_raw.split(' ')
		peer_no = str(idx + 1)
		log = open('log_%s.txt' % (peer_no,), 'w')
		p = subprocess.Popen(arg_final, stdout=log, stderr=log)
		procs.append(p)

	print('test running..')
	running = True
	while running:
		n_r = 0
		for p in procs:
			poll = p.poll()
			if poll is not None:
				print('process went down', poll)
			else:
				n_r += 1

		if n_r == 0:
			print('all processes down')
			running = False
except KeyboardInterrupt as e:
	print('interrupted, terminating')
	for p in procs:
		p.kill()
print('done, check the logfiles')
