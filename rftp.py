#!/usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function
import re, os, sys, ftplib, argparse

parser = argparse.ArgumentParser(description='FTP recursively uploading, downloading, and deleting operation scripts', add_help=False)
parser.add_argument('-h', '--host', metavar='host', type=str, help='FTP host')
parser.add_argument('-p', '--port', metavar='port', type=int, default=21, help='FTP port')
parser.add_argument('-u', '--user', metavar='user', type=str, help='FTP port')
parser.add_argument('-w', '--password', metavar='password', type=str, help='FTP password')
parser.add_argument('-m', '--method', metavar='method', type=str, choices=['get', 'put', 'del'], help='FTP operation method(get, put, del)')
parser.add_argument('-l', '--local', metavar='local', type=str, help='Local working directory')
parser.add_argument('-r', '--remote', metavar='remote', type=str, help='Remote working directory')

class RetrLines(list):
	def retrLine(self, retr):
		if retr[-2:] == ' .' or retr[-3:] == ' ..':
			return
		
		item = dict(zip(['perms', 'number', 'owner', 'group', 'size', 'month', 'day', 'year/time', 'name'], re.split(r'[\s]+', retr, 8)))
		item['isdir'] = item['perms'][0] == 'd'
		item['islink'] = item['perms'][0] == 'l'
		item['size'] = int(item['size'])
		item['number'] = int(item['number'])
		item['owner'] = int(item['owner'])
		item['group'] = int(item['group'])
		
		if item['islink']:
			names = re.split(r'[\s]+\-\>[\s]+', item['name'])
			item['name'] = names[0]
			item['link'] = names[1]
		self.append(item)

def ftpget(f, local, remote):
	for item in ftpdir(f, remote):
		plocal = local + '/' + item['name']
		premote = remote + '/' + item['name']
		if item['isdir']:
			sys.stdout.flush()
			if os.path.isdir(plocal):
				print(' Created Directory %s skip\n' % plocal, end='')
			else:
				os.mkdir(plocal)
				print(' Created Directory %s success\n' % plocal, end='')
			
			sys.stdout.flush()
			
			if item['number']>0:
				ftpget(f, plocal, premote)
		elif item['islink']:
			try:
				os.symlink(item['link'], plocal)
				print('   Created symlink %s success\n' % plocal, end='')
			except OSError:
				print('   Created symlink %s skip\n' % plocal, end='')
			
			sys.stdout.flush()
		else:
			if os.path.isfile(plocal) and item['size'] == os.path.getsize(plocal):
				print('   Downloaded file %s skip\n' % plocal, end='')
			else:
				print('  Downloading file %s ...\r' % plocal, end='')
				sys.stdout.flush()
				fp = open(plocal, 'ab')
				if item['size'] > 0 and item['size'] > fp.tell():
					f.retrbinary('RETR %s' % premote, fp.write, 1024)
				
				fp.close()
				print('   Downloaded file %s success\n' % plocal, end='')
			
			sys.stdout.flush()

def ftpput(f, local, remote):
	pass

def ftpdel(f, remote):
	pass

def ftpdir(f, remote):
	retrLines = RetrLines()
	f.dir('-a ' + remote, retrLines.retrLine)
	return retrLines

def main():
	args = parser.parse_args()
	if args.host == None or args.port == None or args.user == None or args.password == None or args.method == None or (args.method != 'del' and args.local == None) or args.remote == None:
		parser.print_help()
		return
	
	f = ftplib.FTP()
	f.connect(args.host, args.port)
	f.login(args.user, args.password)
	
	if args.method != 'del' and not os.path.isdir(args.local):
		os.mkdir(args.local)
	
	if args.method == 'get':
		ftpget(f, args.local, args.remote)
	elif args.method == 'put':
		ftpput(f, args.local, args.remote)
	else:
		ftpdel(f, args.remote)
	
	f.quit()

if __name__ == '__main__':
	main()

