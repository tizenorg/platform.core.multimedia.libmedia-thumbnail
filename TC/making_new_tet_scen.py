#!/usr/bin/python

#############################################
# tet_scen  auto generator
#
# ** argv[1] = TC root
#############################################

import sys,string,os


write_file = open("tet_scen", 'w')

#
# making new tet_scen
#
def making_tet_scen (filename):
	#tmp_list = filename.strip().split('/')
	n_filename = filename.replace(' ', '\\ ')
	#print n_filename
	#new_path = "/"+ sys.argv[1] +"/"+n_filename[2:-6]
	new_path = "/"+n_filename[:-6]
	#print new_path
	file = open(filename, 'r')
	lines = file.readlines()
	for line in lines:
		if len(line.strip()) > 1:
			list = line.strip().split('/')
			#print new_path + list[-1]
			write_file.write("\t"+new_path+list[-1]+"\n")

#
# usage()
#
def usage():
	print(" ")
	print("./making_new_tet_scen.py tc_root")
	print("Put the Test Case's root directory.")
	print("Do not include '/' at the end")
	print(" ")

#
# main()
#
def main():
	if len(sys.argv) < 2:
		usage()
		sys.exit(0)
		
	os.system('find '+ sys.argv[1] +' -name "tslist" > tslist.txt')
	
	#write_file = open("tetscen", w)
	write_file.write("# auto generated tet_scen\n")
	write_file.write("all\n")
	write_file.write("\t\"Starting Full Test Suite\"\n")
	
	for file in open("tslist.txt", 'r'):
		#print file.strip()
		making_tet_scen(file.strip())
			
	write_file.write("\t\"Completed Full Test Suite\"\n")
	write_file.write("# EOF\n")
	write_file.close()
	print(" ")
	print("==============================")
	print("New tet_scen file is made~~~~")
	print("==============================")
	print(" ")
main()
