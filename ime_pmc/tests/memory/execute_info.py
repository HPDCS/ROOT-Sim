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

def writeOvh(fileName, agg) :

	ovhFile = open(DIR_RESULT + '/' + DIR_INFO + '/' + fileName + EXT, 'w+')

	for line in agg :
		ovhFile.write(line + '\n')

	ovhFile.close()

def writeInfo(fileName, info) :
	
	infoFile = open(DIR_RESULT + '/' + DIR_INFO + '/' + fileName + EXT, 'a+')
	infoFile.write(info + '\n')

	infoFile.close()

def clearInfo(fileName) :
	open(DIR_RESULT + '/' + DIR_INFO + '/' + fileName + EXT, 'w').close()

FILE_ORIG = 'main.c.origin'
FILE_MAIN = 'main.c'
FILE_COPY = 'main.c.bkp'
MAGIC_CODE = '#INJECT'

FREQUENCIES = ['0x0', '0x1', '0x4', '0x10', '0x100', '0x1000', '0x10000']
#FREQUENCIES = ['0x10000']
ISTR_CTN = [0 , 1, 2, 5, 10, 25, 50, 100]
#ISTR_CTN = [0]

def main() :
    array = [0, 0, 0, 0, 0, 0, 0, 0]
    index = 0
    readFile = open(DIR_RESULT + '/' + DIR_INFO + '/' + 'oracle_info' + EXT, 'r')
    for line in readFile:
        s = int(line.split('\t')[1])
        array[index] = s
        index += 1
    for ctn in ISTR_CTN:
        index = 0
        aggregates = [0, 0, 0, 0, 0, 0, 0]
        readFile = open(DIR_RESULT + '/' + DIR_INFO + '/' + 'ime' +str(ctn) + EXT, 'r')
        for line in readFile:
            s = int(line.split('\t')[1])
            s = float(s)/float(array[index])
            line = FREQUENCIES[index] + '\t' + str(s)
            aggregates[index] = line
            index += 1
        writeOvh('ovh' + str(ctn), aggregates)

	# cmd(['gnuplot', '../plot.plt'])


if __name__ == "__main__":
	main()
