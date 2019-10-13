
bix-blockchain
==============

Essentially distributed and privately owned by governments, authorities, agencies, associations, banking/non-banking financial institutions, business entities, IoT, universities, colleges, schools and other organizations, BIX Blockchain is enabling its members, employees, students using BIX network for business, financial, payment, security, educational, gaming and other purposes.

The canonical Blockchain is stored on “consensus Layer0” servers, where each server has a copy of the whole Blockchain. Initially BIX Blockchain’s main purpose was using it as the decentralized database for record   storage, while   customers   can connect to any of our Core Servers to login. In case of using BIX (Blockchain Information eXchange) API, they can continuously send requests for insertion of the data into the Blockchain,	immediately	followed	by acknowledgement for each request. 

For more information see: [BLOCKCHAIN INFORMATION.pdf](https://raw.githubusercontent.com/BIX-Club/bix-blockchain/master/doc/BLOCKCHAIN%20INFORMATION.pdf)


BIX Blockchain software consists of two layers:

- Layer0 (core) servers are servers responsible for the
blockchain. They receive messages to be inserted into the blockchain,
they store canonical version of the blockchain and are responsible for
finding a concensus about new blocks. When requested they send
informations (like the content of a given block) to layer1 servers.

- Layer1 servers usually provide a GUI for the blockchain. Layer1
servers get requests from users and submit corresponding messages to
layer0 servers to be inserted into the blockchain. That are messages
like new account creation, money trasfer, new token creation, etc.
They can also request Layer0 server for the content of any accepted
block and/or the current state of internal databases. For example,
they can request current balance of an account, content of currently
mined block for proof of work mining, etc.



Layer0: blockchain database
---------------------------

BIX blockchain layer0 software runs on multiple servers (let's say N). It
receives messages (transactions) and store them into a blockchain
database. All servers will store exactly the same copy of the
blockchain database. This redundancy makes it difficult to falsify the
historical data stored in the blockchain (an attacker would need to
control more than N/2 servers during some period of time). Each block
contains a hash of the previous block and is digitally signed by at
least N/2+1 layer0 servers. Once the information is inserted into the
blockchain it is practically impossible to remove or alter it.

Contrary to bitcoin, this software is not supposed to create a unique
"world wide" network where any number of computers are producing one
unique world wide chain of data. We suppose that the software will be
used on a predefined set of core computers.  One blockchain database
will be created by a predefined network of servers. Servers will have
fixed IP addresses, unique pairs of private/public keys and
connections will be secured by usual certificates.  It is supposed
that servers maintaining a single blockchain will belong and will be
maintained by several independent trusted companies, governmental
agencies, banks, etc.. The credibility of the blockchain will be
derived from the credibility of owners of layer0 servers.

However, different consortiums of institutions can create different
networks for their own purposes. They can use the same software to
create different blockchain databases.  We suppose that the technology
can be used in many circumstances giving raised a number of
specialized networks (depending on layer1 application).

Layer0 servers connects to a predefined subset of other layer0 servers
and the whole topology of connections is known in advance. BIX
protocol allows relaying of messages meaning that a server can send
any information to another server even if they are not directly
connected.  It allows to connect servers which are behind firewalls
and are inaccessible from the Internet. An ideal topology for layer0
servers consists of a ring of central servers visible on the Internet
(and probably connected each to each) and of a set of private servers
behind firewalls where private servers initiate connections to a few
of servers of the central ring. All servers are continuously
broadcasting dynamic informations about their live connections
including latencies, hence every server knows the current fastest path
to any other server.  It is envisaged that the number of servers and
the topology can be dynamically changed based on network consensus,
however it is supposed that such changes will be rare.

Any of the servers can receive a message which will be later stored
into the blockchain. When receiving a message, the layer0 server
assigns a timestamp and an ID to this message and broadcasts the
content of the message together with the ID and timestamp to all other
layer0 servers.  Broadcasting of messages is independent on consensus
algorithm described later.

The consensus algorithm for layer0 servers is designed in a way to
allow maximal data flow. I.e. the protocol of finding the consensus is
trying to minimize the amount of data exchanged between servers. The
main idea of the concensus is simple. Let's say we have N core
servers. At a random time one of core servers decides to issue a new
block, it announces the intention to the network and other servers
confirms that they are ready to accept this node. Once the proposal
collects N/2+1 confirmations, the vote is passing to the next
round. In rare cases it may happen that more than 1 server decides to
issue a new block at the same time and none of proposals gets N/2+1
confirmations. In such case the proposal expires after some time (1
second right now) and servers are trying to (re)confirm the best of
received proposals. This happens until one of proposals gets N/2+1
confirmations.  If the server receives a positive feedback from N/2+1
servers, it passes to second round by broadcasting IDs of messages
stored in the proposed block and the total hash of bodies. Similarly
to previous round it waits for confirmations from other servers. If
N/2+1 servers confirms that they have the correct content of all
listed messages, the third voting round is performed. Third round
checks that the content of included messages is internally consistent
(for example if money transfer orders are correctly signed by account
holders and accounts have enough of balance to make transaction). If
all the messages of the block are consistent on at least N/2+1 servers
the fourth (last) round starts. In this round each Layer0 server signs
the whole block with its private key and other servers checks the
validity of signatures using known public keys. When at least N/2+1
valid signatures are collected, the block is inserted into the
blockchain. Each voting round has its own timeout during which it must
be finished, otherwise the whole block is rejected and consensus
starts from the beginning.

In normal case the network finds a consensus in a fraction of second.
In case of a conflict it can take a few seconds.  The speed we are
targeting is to issue one block per let's say 10 seconds. The value is
customizable (longer period decreases server load and allows smoother
consensus). We estimate that 10 seconds is the time while a human user
can wait in front of a computer for processing of his
message/transaction. 

All layer0 servers have equal position in the network (no one is
designed as the main server, no one is a slave server). Layer1 servers
can connect to any layer0 server completely transparently. The network
will continue to work if any of servers is turned off unless there is
more than N/2 servers down at the same moment. In such a case the
network will block until at least N/2 servers are turned on. This
ensures that the software will continue to work without interruption
in case of a normal maintenance or hardware failure of one of
servers.



Layer1: Using the database
--------------------------

There are many possibilities how to use blockchain database. It is
supposed that there will be a network of layer1 computers which will
use the underlying blockchain technology to store their data. The data
will be signed and/or encrypted by public/private keys depending on
the application by which they are produced and used. The format of
messages stored in blockchain will be under responsibility of the
layer1 computer. However, there is a builtin support in layer0 servers
for some types of messages like financial transactions.  Support for
another builtin messages can be achieved only by modifying the code of
the layer0 servers.  Messages stored in the blockchain are available
to anybody for verification. Of course in case of encrypted messages,
the person willing to verify the content will need the corresponding
decryption key.


Some of possible scenarios for layer1 software are:


1) A banking systems:

The blockchain can be used to store information of kind "User X has
$Y on his account". It can also be used to store information like
"User X has transferred $Y to user Z" which is basically the
information required to manage a money account system.


2) Virtual currency:

Similarly to previous point, if accounts are identified by a public
key and money orders are signed by private keys the blockchain can
maintain a banking system of a virtual currency.


3) Register of land owners:

If a person X buys a land (or house, etc.) a governmental server will
insert a message saying "X owns house at Street Y, number 123, city,
state" into the blockchain. Because the blockchain can not be
falsified such information shall have the same weight as a paper
certificate.


Business model
--------------

In accordance with the previous points, the current implementation
contains a builtin support for insertion of an arbitrary user file
into the blockchain, performing financial transactions and creation
and usage of custom new virtual currencies.

The implementation contains support for a business model where owners
of layer0 servers receive recompensation for offering disk space and
running the network and users entering information into the blockchain
pay fees depending on the size and the type of stored information.




