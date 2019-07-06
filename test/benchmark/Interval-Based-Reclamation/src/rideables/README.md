Each data structure consists of at least a .hpp file
and (usually) a matching .cpp file.  Brief descriptions
follow below.

### SGLUnorderedMap 

A std::unordered_map protected by a TAS lock.

### SortedUnorderedMap

A 1000000-bucket list-based hash map according to 
Michelle[2002]. In each bucket, nodes are kept in order 
by their keys.

Two versions included. Range version for TagIBR and
the basic version for others.

### LinkList

A 1-bucket SortedUnorderedMap. In each bucket, 
nodes are kept in order by their keys.

Two versions included. Range version for TagIBR and
the basic version for others.

### NatarajanTree

A lock-free Binary Search Tree according to 
Natarajan[2014]. It is an ordered map.

Two versions included. Range version for TagIBR and
the basic version for others.

### BonsaiTree

A state-based lock-free Binary Search Tree, an ordered
map. Every write operation creates a new state and 
rebuild the whole tree. Reads are wait-free. The 
prototype of Bonsai Tree is by A. T. Clements in 
ASPLOS'12.

Two versions included. Range version for TagIBR and
the basic version for others.