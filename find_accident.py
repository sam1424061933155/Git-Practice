# -*- coding: utf-8 -*-
import csv
import sys
f = open("data/out_temp1.csv","r")
f1 = open("data/acc_temp.csv","r")
f2 = open("traffic2PM.csv","w")
w = csv.writer(f2)
ans=[]
reload(sys)
sys.setdefaultencoding('utf-8')
for row in csv.reader(f):
	print("in out")
	print(row)
	del ans[:]
	for row1 in csv.reader(f1):
		#count+=1
		#print(row1)
		if((row[0]==row1[0])and(row[1]==row1[1])):
			temp=row1[2]
			if(int(temp)<10):
				temp="0"+temp
			if(row[2]==temp):
				#print(row1[3])
				ans.append(row1[3])
				w.writerow(ans)
				print("in acc")
				print(row1[3])
				break
f.close()
f1.close()
				
				
	

	
	