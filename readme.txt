NAME:
Qiujiang Jin

DESIGN:
In each block there is a head at the bottom and a foot at the top. I store the size and alloc information in both head and foot. And all free blocks are stored in a double linked list. So all sizes will be adjusted to be at least 16 bytes so that the free block could hold 2 pointers(one points to the previouse block and the other points to the next block). We also have 2 global pointers base and end to store the starting and the ending position fo the heap. In the mymalloc I find the first free block in the linked list from the head whose size is larger than the requested size and allocate in this free block. If the size of this free block is sufficiently large, I split it into one allocated block and the other free block. In the myfree I use the merge free blocks functions to merge adjacent free blocks into one free block. And we must insert and delete node in the linked list accordingly. In the myralloc if the new szie is sufficiently smaller than the old size I also split the old block. If the new szie is larger than the old size, I free the old block and allocate the new one then copy the old content into the new one.

RATIONALE: 
I use the double linked list to store the free blocks so I can search for the free block fast. I use double linked list so that it will be convenient to delete a node. This is because when I need to delete a node, I must know both the previouse node and the next node. And I use the merge free blocks function which is enlightened by the coalesce function in the textbook. So I set both head and foot in the block which can help us to determine if the previouse or next block is allocated or freed. Notice that in the allocating ot mergin free blocks case, I must delete ot insert nodes in the free linked list so that this list can represent the right conditions of the free blocks. 

OPTIMIZATION: 
I change the GCC optimization flags to -O3 which can lead to the best time. In the beginning when I need to delete a node and insert a new node, I use a replace function that replace the current node with the new node in the same position in the linked list. The utility is 70% and the time is 70%. Then I don't use the replace function and just delete the node and insert the new node at the head of the linked list. Then the utility decrease to 66% but the time increased to 80%. This is a good improvement.

EVALUATION:
In my final versin both the utility and the time has reached the standard of the full credit. But If I can use more than one free double linked list so that the free blocks with the similar sizes can be stored in one list, I can improve the time because in this case I may spend less time to search a proper free block.

WHAT WOULD YOU LIKE TO TELL US ABOUT YOUR PROJECT?
I do this project by myself(though I was influenced by the textbook and the lecture given in this class) and reach ths full credit standard in both utility and time. I think this is an acceptable result for me.

REFERENCES:
I use the idea given in the lecture of CS107. I also read the chapter 9.9-9.11 in the B&O textbook and research its code carefully. So in my own code the write_size function, the read_size function and the read_alloc function are influenced by the code in the textbook. In my merge_free_block function I realize its functionality influenced by the coalesce function in the 9.9.12 in the B&O textbook. This is the reference of my code.
