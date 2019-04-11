import os
import subprocess
import sys, getopt
import glob


# UTILITY for CMD RESULTS
OUT = 0
ERR = 1
RET = 2

def cmd(cmd_seq, type=None, sh=False) :
	p = subprocess.Popen(cmd_seq, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=sh)
	p.wait()
	if type == OUT or type == ERR :
		return p.communicate()[type].strip().split('\n')
	elif type == RET :
		return p.returncode
	else :
		err, out = p.communicate()
		return err.strip().split('\n'), out.strip().split('\n'), p.returncode

THRESHOLD = 1 << 46

DIR_RESULT = 'results'
DIR_INFO = 'info'
DIR_PLOTS = 'plots'
DIR_DATA = 'data'

EXT = '.dat'


def check_dir(names) :
	for name in names :
		if os.path.exists(name) :
			ver = len([e for e in cmd(['ls'], OUT) if name in e]) #filter(lambda e: DIR_LOG in e, cmd(['ls'], OUT)))
			cmd(['mv', name, name + '.' + str(ver)])
		cmd(['mkdir', name])

def clear_dir(names) :
	for name in names :
		cmd('rm -r ' + name + '*', sh=True)[0]


def writeList(fileName, pages) :

	listFile = open(DIR_RESULT + '/' + DIR_DATA + '/' + fileName + EXT, 'w+')
	
	for stat in pages :
		listFile.write(stat + '\n')

	listFile.close()

def writeAgg(fileName, agg) :

	aggFile = open(DIR_RESULT + '/' + DIR_DATA + '/' + fileName + EXT, 'w+')

	for line in agg :
		aggFile.write(line + '\n')

	aggFile.close()

def writeInfo(fileName, info) :
	
	infoFile = open(DIR_RESULT + '/' + DIR_INFO + '/' + fileName + '.txt', 'w+')
	infoFile.write(info + '\n')

	infoFile.close()

FILE_ORIG = 'main.c.origin'
FILE_MAIN = 'main.c'
FILE_COPY = 'main.c.bkp'
MAGIC_CODE = '#INJECT'

FREQUENCIES = ['0x10000']#['0x0', '0x1', '0x4', '0x10', '0x100', '0x1000', '0x10000']

ISTR_CTN = ['0']#, '1', '2', '5', '10', '25', '50', '100']

def main() :


	try:
		opts, args = getopt.getopt(sys.argv[1:], "c", ['clear']) # ["help", "output="])
	except getopt.GetoptError as err :
		print (str(err))
		exit(1)
	for o, a in opts:
		if o in ("-c", "--clear") :
			clear_dir([DIR_RESULT + '/' + DIR_INFO, DIR_RESULT + '/' + DIR_DATA, DIR_RESULT + '/' + DIR_PLOTS])
			return 0

	
	for ctn in ISTR_CTN :
		print (cmd(['cp', FILE_ORIG, FILE_COPY]))
		
		
		srcFile = open(FILE_COPY, 'r')
		outFile = open(FILE_MAIN, 'w')

		for line in srcFile :
			if MAGIC_CODE in line :
				if (ctn > 0) :
					outFile.write('asm volatile (\n')
					for i in range(ctn) :
						outFile.write('"xchgq %rax, %rbx\\n"\n')
						outFile.write('"xchgq %rbx, %rax\\n"\n')
					outFile.write(');\n')
					
			else :
				outFile.write(line)



		srcFile.close()
		outFile.close()
		



		res = cmd(['../../main/profiler', '-n']) # accende
		if (res[RET] != 0) :
			print('Cannot activate profiler')

		agg = ['#', 'Memory', 'Total']

		os.chdir(DIR_RESULT)

		check_dir([DIR_INFO, DIR_DATA, DIR_PLOTS])
		
		os.chdir('..')

		for freq in FREQUENCIES :	
			res = cmd(['../../main/profiler', '-s', freq]) # imposta la frequenza freq
			if (res[RET] != 0) :
				print('Cannot set frequency')


			res = cmd('./hot_page 1 1', sh=True)

			if (res[RET] != 0) :
				print('something wrong')
				return -1

			# This returns 2 values: [0] ID, [1] Exec time (ms)
			pid, time = res[OUT][3].split()


			res = cmd(['../../main/profiler', '-t', pid]) # registra il thread pid
			if (res[RET] != 0) :
				print('Cannot print stats')

			print(res[OUT]) # non ti serve tutto agg...
			# Create the aggregate file
			agg[0] += '\t' + freq
			# The 4^ element is the # of memory samples
			agg[1] += '\t' + res[OUT][3].split()[1]
			# The 5^ element is the # of total samples
			agg[2] += '\t' + res[OUT][4].split()[1]



			res = cmd(['../../main/profiler', '-x', pid]) # legge le statistiche
			if (res[RET] != 0) :
				print('Cannot print stats')

			# The first 3 elements are information string, so filter them out
			rawList = res[OUT][3:]

			# Sort and filter out metadata access memory samples
			fineList = sorted(filter(lambda x: x.startswith('0x4'), rawList))
				
			writeList('hop' + freq, fineList) # ti interessa questo
			writeInfo('hop' + freq, time)

		writeAgg('agg', agg) # non ti serve

		cmd(['touch', DIR_RESULT + '/' + DIR_INFO + '/' + ctn + '.inf']) # non ti serve
		# os.chdir(DIR_RESULT + '/' + DIR_DATA)

	# cmd(['gnuplot', '../plot.plt'])
	# cmd(['gnuplot', '../agg.plt'])
	# cmd(['mv', '*.png', '../' + DIR_PLOTS])


if __name__ == "__main__":
	main()
