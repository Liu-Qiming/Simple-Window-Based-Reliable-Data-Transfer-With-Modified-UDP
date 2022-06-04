# CS118 Project 2

This is the repo for spring 2022 cs118 project 2.

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` USERID to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.


## High level design
Server side: For SR, we are keeping track of a window and has every received pkts to be marked as received. Every out of order pkts or invalid pkts will be
ignored so that we can send the dupack to the client side. When the connection is not over, we are sending the next expected pkt number after received a 
new pkt. We also keep a buffer which can contain the window size number of pkts' payload. When there is a pkt is missing, we move our window until that 
lost pkt's number, and in the meantime only write the received pkts' payload to the buffer when the currect start pkt is equal to the cap number of pkt within the window. If there is no pkt lossing, we write incoming payload to the buffer, but if there is pkt lossing, we slide the window forward to have the 
missing pkt takes up the first place on the bitmap and waiting for client's retransmission.

Client side: in addition to the pkts array that is already given in the skeleton, we have a pkts_timer array to keep track of the timers of the packets in the window, and a pkts_ack_received array to record whether this sent packet has been received or not. Note here we also have a pkt_real_num: {0, 1, 2, 3, 4… }

Implementation:
The first thing we do in the big while loop is to see if the whole process is done. If there is no more pkt to send and we have received the ack of all the sent packets, we just break the big loop and finish. 

If we still have packets to send and the window is not full, we keep sending packets until the window is full. Here, we set the timer of all sent packets to be the current time and set their ack_received to to be false. Note that we stop sending when there is no more data to send (even when the window is still not full.)

Then we start receiving. Here we have a small receiving loop. The only case when we can break and leave this small receiving loop is when we receive an ack with valid content. If we are not receiving anything, we check all the packets in the window to see if there is a timeout. If the received flag for that packet is false and it is timeout, we resend that specific packet (selective repeat.) We keep doing this checking timeout and receiving until we receive an ack and break this small receiving loop.

After we exit the small receiving loop (which means we have received sth,) see which packet is the ack for, get its pkt_real_num, and get its corresponding received flag to true. Now, check if this packet is the 1st one in the window (get idx by pkt_real_num - s.) If yes, then we can start shifting the window. To shift the window, find the 1st packet in the window that has a false received flag. This tells us how much to shift (shift_size.) Then, we just shift all of our three arrays (pkts, pkts_timer, pkts_ack_received) by this shift_size. 

If the received packet is not the first one in the window, then we can not do any shifting and should just go back to the keep receiving & timeout checking. 


## Problem 
Server side: when we were doing the sliding window on the server side, we were choosing between two strategies: move all received pkts to the front by 
doing arr[i]=arr[i+1] until a lost pkt, and set to zero for all bit map afterwards. However, this approach would introduce bugs such as seg fault, and 
the pkts' bitmap were not moved correspondingly. We solved it by using another direct method by using memory accessing and copying. I believe there were 
some indexing problem to the previous approach. Moreover, we were handling the incoming pkt which was lost incorrectly in the first place. When a pkt is 
lost, we should keep write in buffer and sendout ack num until we reach that missed pkt. We need to keep the receiving window and the receiving buffer 
cohesively o/w it might cause out-of-order write to the output file. 
In the GBN we implemented previously, the synack and finack were constantly lossing. I set a force continue after several times of timeout to force
resolved this problem. Our ACK number was also able to receive, and we used the newACK-oldACK mod 512 in the first place, but we realized that this
might introduce too much complexity, so we switched to using our window pkts[] to linear search through this array to see if there are any pkts that
we jumped. Because if the server returns a larger acknum then it cannot be pkt loss, i.e. the server mush receive it and the ack was lost.