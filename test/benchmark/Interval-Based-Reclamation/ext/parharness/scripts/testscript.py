#!/usr/bin/python


# Copyright 2017 University of Rochester

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 



from os.path import dirname, realpath, sep, pardir
import sys
import os

# path loading ----------
#print dirname(realpath(__file__))
os.environ['PATH'] = dirname(realpath(__file__))+\
":" + os.environ['PATH'] # scripts
os.environ['PATH'] = dirname(realpath(__file__))+\
"/..:" + os.environ['PATH'] # bin
os.environ['PATH'] = dirname(realpath(__file__))+\
"/../../../bin:" + os.environ['PATH'] # metacmd

#"/../../cpp_harness:" + os.environ['PATH'] # metacmd

# execution ----------------
for i in range(0,5):
	cmd = "metacmd.py main -i 10 -m 3 -v -r 1 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=QSBR:tracker"+\
	" -o data/final/hashmap_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 5 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=LF:tracker=FAA:tracker=WCAS"+\
	" -o data/final/hashmap_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 2 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=QSBR"+\
	" -o data/final/list_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 6 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=LF:tracker=FAA:tracker=WCAS"+\
	" -o data/final/list_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 3 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=QSBR:tracker=Interval"+\
	" -o data/final/bonsai_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 7 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=LF:tracker=FAA:tracker=WCAS"+\
	" -o data/final/bonsai_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 4 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=QSBR"+\
	" -o data/final/natarajan_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -m 3 -v -r 8 -demptyf=1"+\
	" --meta t:1:5:10:15:20:25:30:35:40:45:50:55:60:65:70:75:80:85:90:95:100"+\
	" --meta d:tracker=LF:tracker=FAA:tracker=WCAS"+\
	" -o data/final/natarajan_result.csv"
	os.system(cmd)
	