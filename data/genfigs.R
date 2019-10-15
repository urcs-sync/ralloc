# Copyright 2015 University of Rochester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 

###############################################################
### This script generates the 8 plots that were actually    ###
### used in the paper using the data contained in ../final/ ###
###############################################################

library(plyr)
library(ggplot2)

filenames<-c("threadtest","shbench","prod-con")
for (f in filenames){
f_names<-dir(paste("./",f,"/",sep=""), full.names=TRUE)
lindata<-do.call(rbind,lapply(f_names,read.csv,header=TRUE,row.names=NULL))

lindata$allocator<-as.factor(gsub("lr","LRMalloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("r","Ralloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("mak","Makalu",lindata$allocator))
lindata$allocator<-as.factor(gsub("je","JEMalloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("pmdk","libpmemobj",lindata$allocator))
lindata$allocator<-as.factor(gsub("mne","Built-in allocator",lindata$allocator))

ddply(.data=lindata,.(allocator,thread),mutate,mem=mean(rss)/1024)->lindata
ddply(.data=lindata,.(allocator,thread),mutate,time= mean(exec_time))->lindata
lindata$allocator <- factor(lindata$allocator, levels=c("Ralloc", "Makalu", "Built-in allocator", "libpmemobj", "LRMalloc", "JEMalloc"))
# Set up colors and shapes (invariant for all plots)
color_key = c("#C11B14","#1245EA","#FF69B4", 
               "#FF8C00", "#12E1EA", "#1BC40F")
names(color_key) <- levels(lindata$allocator)

shape_key = c(18,1,0,3,2,62)
names(shape_key) <- levels(lindata$allocator)

line_key = c(1,2,5,5,4,4)
names(line_key) <- levels(lindata$allocator)


###########################################
#### Begin charts for Time Consumption ####
###########################################

# legend_pos=c(0.5,0.92)
# y_range_down = 0
# y_range_up = 2000

# Benchmark-specific plot formatting
legend_pos=c(0.5,0.92)
y_range_up=NA
if(f=="shbench"){
  y_range_up=105
} else if(f=="prod-con"){
  y_range_up=15
} else if(f=="threadtest"){
  y_range_up=600
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=thread,y=time,color=allocator, shape=allocator, linetype=allocator))+
  geom_line()+xlab("Threads")+ylab("Time (second)")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$allocator])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$allocator])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$allocator])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,48),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,48))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))

# Save all four plots to separate PDFs
ggsave(filename = paste("./",f,"_linchart.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)


#######################################
#### Begin charts for Memory Usage ####
#######################################
# Benchmark-specific plot formatting
legend_pos=c(0.5,0.92)
y_range_up=NA
if(f=="shbench"){
  legend_pos=c(0.415,0.92)
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=thread,y=mem,color=allocator, shape=allocator, linetype=allocator))+
  geom_line()+xlab("Threads")+ylab("Memory Usage (MB)")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$allocator])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$allocator])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$allocator])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,48),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,48))+
  # scale_y_continuous(limits = c(0, y_range_up))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))

# Save all four plots to separate PDFs
ggsave(filename = paste("./",f,"_memory.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)
}



# for larson and memcached
filenames<-c("larson")
for (f in filenames){
f_names<-dir(paste("./",f,"/",sep=""), full.names=TRUE)
lindata<-do.call(rbind,lapply(f_names,read.csv,header=TRUE,row.names=NULL))

#lindata$allocator<-as.factor(gsub("emptyf=1:","",lindata$allocator))
if(f=="memcached"){
  ddply(.data=lindata,.(allocator,thread),mutate,mops= mean(ops)/1000)->lindata
} else {
  ddply(.data=lindata,.(allocator,thread),mutate,mops= mean(ops)/1000000)->lindata
}
lindata$allocator<-as.factor(gsub("lr","LRMalloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("r","Ralloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("mak","Makalu",lindata$allocator))
lindata$allocator<-as.factor(gsub("je","JEMalloc",lindata$allocator))
lindata$allocator<-as.factor(gsub("pmdk","libpmemobj",lindata$allocator))
lindata$allocator<-as.factor(gsub("mne","Built-in allocator",lindata$allocator))

ddply(.data=lindata,.(allocator,thread),mutate,mem=mean(rss)/1024)->lindata
lindata$allocator <- factor(lindata$allocator, levels=c("Ralloc", "Makalu", "Built-in allocator", "libpmemobj", "LRMalloc", "JEMalloc"))

names(color_key) <- levels(lindata$allocator)

names(shape_key) <- levels(lindata$allocator)

names(line_key) <- levels(lindata$allocator)


#####################################
#### Begin charts for Throughput ####
#####################################

# legend_pos=c(0.5,0.92)
# y_range_down = 0
# y_range_up = 2000

# Benchmark-specific plot formatting
legend_pos=c(0.5,0.92)
y_range_up=95
y_name="Throughput (M ops/sec)"


# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=thread,y=mops,color=allocator, shape=allocator, linetype=allocator))+
  geom_line()+xlab("Threads")+ylab(y_name)+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$allocator])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$allocator])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$allocator])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,48),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,48))+
  coord_cartesian(ylim = c(0, y_range_up))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))

# Save all four plots to separate PDFs
ggsave(filename = paste("./",f,"_linchart.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)



#######################################
#### Begin charts for Memory Usage ####
#######################################
# Benchmark-specific plot formatting
legend_pos=c(0.5,0.92)
if(f=="larson"){
  legend_pos=c(0.45,0.92)
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=thread,y=mem,color=allocator, shape=allocator, linetype=allocator))+
  geom_line()+xlab("Threads")+ylab("Memory Usage (MB)")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$allocator])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$allocator])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$allocator])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,48),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,48))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))

# Save all four plots to separate PDFs
ggsave(filename = paste("./",f,"_memory.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)
}
