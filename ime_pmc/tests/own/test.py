import os
import subprocess
import sys, getopt
import glob
import timeit

# Utility method to print the commands list
def pr_list(lst) : 
	return ' '.join(str(e) for e in lst)

class mstat(object) :
	def __init__(self, real, lst, txt) :
		self.real = real
		self.lst = lst
		self.txt = txt

class bench (object) :
	freq = []
	thds = []
	def __init__(self, cmd) :
		self.cmd = cmd
	def __str__(self) :
		return '<CMD> ' + self.cmd + ' - <FREQ> ' + str(self.freq) + ' <THDS> ' + str(self.thds)
	def __repr__(self) :
		return str(self)

C_TYPE = '#TYPE'
C_RUNS = '#RUNS'
C_BNCH = '#BNCH'
C_FREQ = '#FREQ'
C_THDS = '#THDS'

def set_conf(fln) :
	_type = None
	_runs = None
	_bencmarks = []
	_c_bench = None
	with open(fln) as f:
		for line in f :
			arg = line.strip().split(' ')
			if (arg[0] == C_TYPE) :
				if _type != None :
					print 'Invalid Configuration: type already set'
					exit(1)
				_type = arg[1].strip()
			elif (arg[0] == C_RUNS) :
				if _runs != None :
					print 'Invalid Configuration: runs already set'
					exit(1)
				_runs = int(arg[1].strip())
			elif (arg[0] == C_BNCH) :
				if _c_bench != None :
					_bencmarks.append(_c_bench)
					_c_bench = None
				_c_bench = bench(pr_list(arg[1:]))
			elif (arg[0] == C_FREQ) :
				if _c_bench == None :
					print 'Invalid Configuration: missing bench for freq'
				_c_bench.freq = arg[1:]
			elif (arg[0] == C_THDS) :
				if _c_bench == None :
					print 'Invalid Configuration: missing bench for thds'
				_c_bench.thds = arg[1:]

	if _c_bench != None :
		_bencmarks.append(_c_bench)
	return _type, _runs, _bencmarks

# UTILITY for CMD RESULTS
OUT = 0
ERR = 1
RET = 2
def cmd(cmd_seq, type=None, sh=False) :
	try:
		p = subprocess.Popen(cmd_seq, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=sh)
		p.wait()
		if type == OUT or type == ERR :
			return p.communicate()[type].strip().split('\n')
		elif type == RET :
			return p.returncode
		else :
			err, out = p.communicate()
			return err.strip().split('\n'), out.strip().split('\n'), p.returncode
	except Exception as e:
		print "An error occurred:", e

def check_dir(names) :
	for name in names :
		if os.path.exists(name) :
			ver = len([e for e in cmd(['ls'], OUT) if name in e]) #filter(lambda e: DIR_LOG in e, cmd(['ls'], OUT)))
			cmd(['mv', name, name + '.' + str(ver)])
		cmd(['mkdir', name])

def clear_dir(names) :
	for name in names :
		cmd('rm -r ' + name + '*', sh=True)[0]

RET_PATH = '../../'
IRQ = 'irq'
NMI = 'nmi'
def load_HOP(type) :
	# we are in ./tests/std/ and need to move to main directory
	os.chdir(RET_PATH)
	# remove and clean HOP
	os.system('./xclean.sh')
	# install HOP irq version
	if type == IRQ :
		os.system('./irq_load.sh')
	elif type == NMI : 
		os.system('./nmi_load.sh')
	else :
		print 'Error in load HOP #type: ' + str(type)
		exit(1)
	# come back test dir
	os.chdir('./tests/own/')

## DIRECTORIES
DIR_LOG = 'log'
DIR_RESULT = 'result'
DIR_PARTIAL = 'partial'
EXT = '.bch'

OFF = '0'
STD_FREQ = [OFF, '4096', '8192', '12288', '16384', '24576', '32768']
STD_THDS = ['1', '2', '4', '8', '16', '32', '64']

TIMECMD = "/usr/bin/time -f '%e, %U, %S' "
PROFILER = RET_PATH + 'main/profiler'
DIR_HOP = '/dev/hop'
HOP_CTL = 'ctl'

def compute_agg(tpls) :
	for e in tpls :
		return tpls[e]

def run_tests(runs, benchmarks) :
	
	print benchmarks
	vj = 0

	exec_time = 0

	for bench in benchmarks :
		if len(bench.freq) == 0 :
			bench.freq = STD_FREQ
		else :
			STD_FREQ = bench.freq

		if len(bench.thds) == 0 :
			bench.thds = STD_THDS
		else :
			STD_THDS = bench.thds

		f_result = open(DIR_RESULT + '/BENCH' + str(vj) + EXT, 'a+')
		f_log = open(DIR_LOG + '/BENCH' + str(vj) + EXT, 'a+')

		print 'Executing ' + bench.cmd

		vj += 1

		f_result.write('## CMD ' + bench.cmd + '\n\n')

		f_log.write('Running: ' + PROFILER + ' -c\n')
		res = cmd([PROFILER, '-c'])[RET]
		f_log.write('Done\n')

		start = timeit.default_timer()

		for freq in bench.freq :

			f_result.write('#FREQUENCY ' + freq + '\n')

			## check and shutdown the profiler
			if freq == OFF :
				err = os.system(PROFILER + ' -f')
				f_log.write(PROFILER + ' -f [RET ' + str(err) + '\n')
			else :
				err = os.system(PROFILER + ' -n -s ' + freq)
				f_log.write(PROFILER + ' -n -s [RET ' + str(err) + '\n')
			if (err != 0) :
				print('Unable to perform ' + PROFILER + ' at ' + freq + '-' + thds)
				exit(1)
			## end HOP cmd

			print 'FREQ: ' + freq

			f_log.write('freq: ' + freq + '\n---------\n\n')

			for thds in bench.thds : 

				f_result.write(' ' + thds)

				# Stats for each run-group
				cur_res = {}

				print '\tTHDS: ' + thds

				f_log.write('thds: ' + thds + '\n')

				for run in range(runs) :

					o_e_r = cmd(TIMECMD + bench.cmd + ' -n ' + thds + '\n', sh=True)
					f_log.write('Executing: ' + TIMECMD + bench.cmd + ' -n ' + thds + '\n')
					f_log.write('[RET]: ' + str(o_e_r[RET]) + '\n')
					f_log.write('[ERR]: ' + str(o_e_r[ERR]) + '\n')
					f_log.write('[OUT]: ' + str(o_e_r[OUT]) + '\n')

					# expected the times of the just run benchmark - need to format
					times = o_e_r[ERR][0].strip().split(', ')
					
					t_devs = cmd(['ls', DIR_HOP], OUT)
					t_devs.remove(HOP_CTL)

					tmp_tid = []
					for dev in t_devs :
						f_log.write('Running: ' + PROFILER + ' -t ' + dev + '\n')
						tid = cmd([PROFILER, '-t', dev], OUT)[0].split(' ')
						f_log.write(str(tid) + '\n')
						f_log.write('SEC: ' + cmd([PROFILER, '-t', dev], OUT)[0])
						f_log.write('Done\n')
						if tmp_tid == []:
							tmp_tid = tid
						else :
							tmp_tid = [str(int(tmp_tid[i]) + int(tid[i])) for i in range(len(tid))]

					if len(tmp_tid) > 0 :
						tmp_tid[0] = '#THDS' + str(thds)


					f_log.write('Running: ' + PROFILER + ' -p\n')
					tmp_cpu = cmd([PROFILER, '-p'])[OUT][0].split(' ')
					f_log.write('CALL: ' + str(tmp_cpu))
					f_log.write('SEC: ' + cmd([PROFILER, '-p'])[OUT][0])
					f_log.write('Done\n')

					# use run as key and real time as param to base comparison on
					cur_res[run] = mstat(times[0], 
						times + tmp_tid + tmp_cpu, 
						' | ' + pr_list(times) + ' | ' + pr_list(tmp_tid) + ' | ' + pr_list(tmp_cpu))

					f_log.write('Running: ' + PROFILER + ' -c\n')
					res = cmd([PROFILER, '-c'])[RET]
					f_log.write('Done\n')
					if (res != 0) :
						print ('[' + str(tm) + '] Something wrong at the end of ' + tp + '-' + bh + '-' + fq + '-' + th)
						exit(1)

					# DONE TIMES LOOP

				elem = compute_agg(cur_res)
				f_result.write(elem.txt + '\n')

			# DONE THDS LOOP

		# DONE FREQ LOOP

		stop = timeit.default_timer()

		print 'Execution time ' + '{0:.3f}'.format(stop - start) + ' sec'

		exec_time += (stop - start)

		f_result.close()
		f_log.close()

	# DONE BENCH LOOP

	print 'The entire test took about ' + '{0:.3f}'.format(exec_time) + ' sec'


def main() :
	_type = None
	_runs = None
	_bencmarks = []

	try:
		opts, args = getopt.getopt(sys.argv[1:], "cf:", ['clear', 'file=']) # ["help", "output="])
	except getopt.GetoptError as err :
		print str(err)
		exit(1)
	for o, a in opts:
		if o in ("-c", "--clear") :
			clear_dir([DIR_LOG, DIR_PARTIAL, DIR_RESULT])
		if o in ("-f", "--file") :
			_type, _runs, _bencmarks = set_conf(a)

	if len(_bencmarks) < 1 :
		exit(0)

	if _type != NMI and _type != IRQ :
		print 'Invalid HOP type'
		exit(1)

	if _runs == None or _runs < 1 :
		print 'Invalid runs value'
		exit(1)

	# check and create directories
	check_dir([DIR_LOG, DIR_PARTIAL, DIR_RESULT])

	# clean, make and load HOP
	load_HOP(_type)

	run_tests(_runs, _bencmarks)























if __name__ == "__main__":
	main()
