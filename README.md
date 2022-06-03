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
lost pkt's number, and in the meantime write the received pkts' payload to the buffer. If there is no pkt lossing, we write incoming payload to the buffer.

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