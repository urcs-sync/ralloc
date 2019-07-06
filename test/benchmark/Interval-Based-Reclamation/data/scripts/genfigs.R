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

filenames<-c("hashmap","bonsai","list","natarajan")
for (f in filenames){
read.csv(paste("../final/",f,"_result.csv",sep=""))->lindata

lindata$environment<-as.factor(gsub("tracker=RCU","Epoch",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=HE","HE",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=Hazard","HP",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=NIL","No MM",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=Interval","POIBR",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=LF","TagIBR",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=FAA","TagIBR-FAA",lindata$environment))
lindata$environment<-as.factor(gsub("tracker=Range_new","2GEIBR",lindata$environment))

# Compute average and max retired objects per operation from raw data
ddply(.data=lindata,.(environment,threads),mutate,retired_avg= mean(obj_retired)/(mean(ops)))->lindata
ddply(.data=lindata,.(environment,threads),mutate,ops_max= max(ops)/(interval*1000000))->lindata

nildatalin <- subset(lindata,environment=="No MM")
rcudatalin <- subset(lindata,environment=="Epoch")
hazarddatalin <- subset(lindata,environment=="HP")
hedatalin <- subset(lindata,environment=="HE")

rangedatalin <- subset(lindata,environment=="TagIBR")
rangefaadatalin <- subset(lindata,environment=="TagIBR-FAA")
rangenewdatalin <- subset(lindata,environment=="2GEIBR")
intervaldatalin <- subset(lindata,environment=="POIBR")

lindata = rbind(nildatalin, rangedatalin, rangefaadatalin, rangenewdatalin, rcudatalin, intervaldatalin, hazarddatalin, hedatalin)
lindata$environment <- factor(lindata$environment, levels=c("No MM", "Epoch", "HP", "HE", "TagIBR", "TagIBR-FAA", "2GEIBR", "POIBR"))

# Set up colors and shapes (invariant for all plots)
color_key = c("#000000", "#12E1EA","#1245EA","#FF69B4", 
               "#1BC40F", "#C11B14", "#FF8C00")
names(color_key) <- unique(c(as.character(lindata$environment)))

shape_key = c(17,0,1,2,62,18,4)
names(shape_key) <- unique(c(as.character(lindata$environment)))

line_key = c(1,1,2,4,4,3,2)
names(line_key) <- unique(c(as.character(lindata$environment)))


##########################################
#### Begin charts for retired objects ####
##########################################

legend_pos=c(0.5,0.92)
y_range_down = 0
y_range_up = 2000

# Benchmark-specific plot formatting
if(f=="bonsai"){
  y_range_down=0
  legend_pos=c(0.5,0.92)
  y_range_up=1500
}else if(f=="list"){
  y_range_down=0
  y_range_up=1000
}else if(f=="hashmap"){
  y_range_up=1700
}else if(f=="natarajan"){
  y_range_up=2000
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=threads,y=retired_avg,color=environment, shape=environment, linetype=environment))+
  geom_line()+xlab("Threads")+ylab("Retired Objects per Operation")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$environment])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$environment])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$environment])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))+
  ylim(y_range_down,y_range_up)

# Save all four plots to separate PDFs
ggsave(filename = paste("../final/",f,"_linchart_retired.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)

#####################################
#### Begin charts for throughput ####
#####################################

legend_pos=c(0.4,0.92)
y_range_down = 0
y_range_up = 100

# Benchmark-specific plot formatting
if(f=="bonsai"){
  y_range_down=0.07
  legend_pos=c(0.5,0.92)
  y_range_up=0.30
}else if(f=="list"){
  y_range_down=0
  y_range_up=0.045
}else if(f=="natarajan"){
  y_range_up=45
}else if(f=="hashmap"){
  legend_pos=c(0.33,0.92)
}

# Generate the plots
linchart<-ggplot(data=lindata,
                  aes(x=threads,y=ops_max,color=environment, shape=environment, linetype=environment))+
  geom_line()+xlab("Threads")+ylab("Throughput (M ops/sec)")+geom_point(size=4)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$environment])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$environment])+
  theme_bw()+ guides(shape=guide_legend(title=NULL,nrow = 2))+ 
  guides(color=guide_legend(title=NULL,nrow = 2))+
  guides(linetype=guide_legend(title=NULL,nrow = 2))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$environment])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  theme(legend.position=legend_pos,
     legend.direction="horizontal")+
  theme(text = element_text(size = 20))+
  theme(axis.title.y = element_text(margin = margin(t = 0, r = 15, b = 0, l = 10)))+
  theme(axis.title.x = element_text(margin = margin(t = 15, r = 0, b = 10, l = 0)))+
  ylim(y_range_down,y_range_up)

# Save all four plots to separate PDFs
ggsave(filename = paste("../final/",f,"_linchart_throughput.pdf",sep=""),linchart,width=8, height = 5.5, units = "in", dpi=300)

}
