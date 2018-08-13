#!/usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function
import re, os, sys, ftplib, argparse

parser = argparse.ArgumentParser(description='FTP recursively uploading, downloading, and deleting operation scripts', add_help=False)
parser.add_argument('-h', '--host', metavar='host', type=str, help='FTP host')
parser.add_argument('-p', '--port', metavar='port', type=int, default=21, help='FTP port')
parser.add_argument('-u', '--user', metavar='user', type=str, help='FTP account')
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
				print('   Created symlink %s failure\n' % plocal, end='')
			
			sys.stdout.flush()
		else:
			if os.path.isfile(plocal) and item['size'] == os.path.getsize(plocal):
				print('   Downloaded file %s skip\n' % plocal, end='')
			else:
				print('  Downloading file %s ...\r' % plocal, end='')
				sys.stdout.flush()
				fp = open(plocal, 'ab')
				if item['size'] > 0 and item['size'] > fp.tell():
					f.retrbinary('RETR %s' % premote, fp.write)
				
				fp.close()
				print('   Downloaded file %s success\n' % plocal, end='')
			
			sys.stdout.flush()

def ftpput(f, local, remote):
	if os.path.isfile(local):
		print('    Uploading file %s ...\r' % remote, end='')

		try:
			size = f.size(remote)
		except:
			size = None
			pass
		
		fp = open(local, 'rb')
		if size == os.path.getsize(local):
			print('     Uploaded file %s skip\n' % remote, end='')
		else:
			print('     Uploaded file %s ...\r' % remote, end='')
			sys.stdout.flush()
			f.storbinary('STOR ' + remote, fp);
			print('     Uploaded file %s success\n' % remote, end='')
		fp.close()
		sys.stdout.flush()
	elif os.path.islink(local):
		print('     Uploaded file %s  is link, cannot upload' % local)
	else:
		try:
			f.cwd(remote)
			print('Creating Directory %s skip' % remote)
		except:
			try:
				f.mkd(remote)
				print(' Created Directory %s success' % remote)
			except:
				print(' Created Directory %s failure' % remote)
		sys.stdout.flush()
		
		for name in os.listdir(local):
			ftpput(f, local + '/' + name, remote + '/' + name)

def ftpdel(f, remote):
	for item in ftpdir(f, remote):
		premote = remote + '/' + item['name']
		if item['isdir']:
			ftpdel(f, premote)
		else:
			try:
				f.delete(premote)
				print('     Deleted file %s success' % premote)
			except:
				print('     Deleted file %s failure' % premote)
			sys.stdout.flush()
	
	try:
		f.rmd(remote)
		print('Deleted Directory %s success' % remote)
	except:
		print('Deleted Directory %s failure' % remote)
	sys.stdout.flush()

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

	if args.remote[0] != '/':
		args.remote = f.pwd().rstrip('/') + '/' + args.remote;
	
	if args.method == 'get':
		if not os.path.isdir(args.local):
			os.mkdir(args.local)
		
		ftpget(f, args.local, args.remote)
	elif args.method == 'put':
		ftpput(f, args.local, args.remote)
	else:
		ftpdel(f, args.remote)
	
	f.quit()

if __name__ == '__main__':
	main()

