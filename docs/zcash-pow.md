Zcash proof of work uses the Equihash PoW. Equihash is an implementation of the generalized birthday problem.

###The Birthday Problem:

We want to first ask the question, given a room with N people in it, what is the probability that at least two of them share the same birthday. This is what will become a hash collision in the practice of the equihash PoW.

Let's solve this for N = 23. 

Assume that there are 365 days in a year, sorry leap year bdays you're excluded because of my laziness.

In probability it is often easier to compute the compliment so we will work with the probability that no two people share a birthday.

P(A) = 1 - P(A`)

When events are independent of each other, the probability of all of the events occurring is equal to a product of the probabilities of each of the events occurring. Therefore, if P(A') can be described as 23 independent events, P(A') could be calculated as

P(A`) = P(e1) x P(e2) x ... P(N)

Each person is considered an independent event of the probability of sharing a birthday with someone else. In equihash this is the probability that hash collision occurs for some n-bit length digest.

Since we are dealing the P(Not sharing a bday) We can compute that for Event 1, there are no previously analyzed people. Therefore, the probability, P(1), that Person 1 does not share his/her birthday with previously analyzed people is 1, or 100%

For Event 2, the only previously analyzed people are Person 1. Assuming that birthdays are equally likely to happen on each of the 365 days of the year, the probability, P(2), that Person 2 has a different birthday than Person 1 is 364/365. This is because, if Person 2 was born on any of the other 364 days of the year, Persons 1 and 2 will not share the same birthday.

We can now compute the P(Not sharing a bday) as...

P(A`) = 365/365 x 364/365 x ... x 365 - N/365
P(A`) = (1/365) x (365 x 364 x ... x 365 - N)

With N = 23, P(A`) = 0.492703. We compute P(A) as

1 - P(A`) = 1 - 0.492703 == 0.507297

So you can expect about a 50% chance of sharing a birthday with someone in a room with 23 people in it.

###Equihash:

Cool, but why is that relevant to equihash? Equihash generalizes this process into an algorithm solve what is called the generalized-birthday, or k-XOR problem, which looks for a set of n-bit strings that XOR to zero. You can think of the birthday problem as telling you what your probability of finding a hash collision is given how many people or hash outputs you have.

Problem:

Given a list L of n-bit strings {Xi} (blake2b 64 hashes expanded to n=200 in practice) find distinct {Xij} such that:

{Xi1} xor {Xi2} xor ... xor {Xi2^k} = 0

Where {Xi} are outputs of the blake2b hash. TODO: Write the exact personalization specs for the zcash blake2b hashes.

blake2b(i1) xor blake2b(i2) xor ... blake2b(i2^k)

Starting with k = 1. We can see that this is just a collision search over the hash output list L. 

Assume we have a hash function with n possible outputs. Then if we apply what we know about the birthday problem, we know that we can expect to have a 50% chance of finding collisions when have the square root of n inputs.

We can expect to find a collision for k=1 with time complexity 2^n/2 when |L| > 2^n/2 which is saying the number of people in the room is |L| and there need to be at least 2^n/2 of them to find a bday collision. Here n is equivalent to the number of days in a year in the birthday problem. So we obtain the probability of each rounds collision event as n - round/n. We want to tune this to where we expect two collisions or solutions on average. Due to the pigeon hole principle, if we had a list of length N of n-bit strings, if N = 2^n we will have a 100% chance of finding a collision. It only takes a list of N=23 in a hash function with 365 outputs to have 50% chance of collision.

There is further theory in the equihash whitepaper on the time-space complexity of this.

###Basic  Wagner’s  algorithm  for  the  generalized

It is advised to read the white paper as all of this understanding was derived from it. 

birthday problem:

Here I describe the process that zcashd and the current open source implementations of zcash mining use.

We consider two parameters for the equihash PoW n and k. Where n = 200 and k = 9.

Create a list L of N n-bit strings. N can be calculated by the following:

N = 2^(n/k+1)+1

Where n = 200, k = 9, N = 2^21 = 2097152

1) Create a table storing {hash: Xj, index: j} for each N.

2) Sort the list L by the hashes Xj and find unordered pairs (i, j) such that Xi collides with Xj on the first n/k+1 bits. Store those as tuples ``(Xi xor Xj, (i, j))`` in a new list L`.

3) Repeat the previous step on the new list L` on the next n/k+1 bits. To do this, we will call n/k+1 the collision_length. We will then loop over the hash on the first bits in range ((i-1)*l/8) to (i*l/8) where l = collision_length and i = the current k round. The 8 in the denominator is to convert bits to bytes. Store the new tuples in L` ` (Xi,j,k,l,  i,j,k,l).

Example:

Round 1 - range = (1-1*20/8) to (1*20/8) = 0 to 20bits

Round 2 - range = (2-1*20/8) to (2*20/8) = 20bits to 40bits

and so on until 2n/k+1 bits or 40 bits remain.


The blake2b output length is a max of 64bytes = 64x8 = 512bits.

4) Find a collision on the last 2n/k+1 bits, and this is your solution.

###Algorithm Binding:

This is where things get a bit complicated.

The generalized birthday problem needs some binding to be a PoW. The goal here is to use the fact that a foot print between list generations is carried over so that people cant supply other colliding bits to the lists between rounds. The intermediate 2^l xors have leading nl/k + 1 leading bits colliding.

###Protocol:

To generate an instance of the proof protocol. 

Select crypto function H and params n, d (difficulty), and k

H = blake2b (With zperson personalization)
n = 200
k = 9
d = 70

Space complexity L is 2^(n/k+1) + k bytes
Time complexity is (k+1)2^(n/k+1)+d calls to the function H
Solution size is 2^k((n/k+1) + 1) + 160 bits
verification costs 2^k XORs

####Generating the list L

We need to select a seed value I which is the hash of the previous block header

example: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

We are asked to provide a 160-bit nonce V and ((n/k+1) + 1)-bit {X1, X2,...X2^k} such that:

H(I||V||X1) XOR H(I||V||X2) XOR ... XOR H(I||V||X2^k) = 0

We need to create a general blake2b64 hash template by initializing the hash with the previous block header and a nonce. Then as we are generate the unique hashes for L, we update the blake2b state again with the index Xi. Then we digest the hash and store it as a tuple of (hash:index) in the list L.

We expect a difficult condition of

H(I||V||X1||X2||...||X2^k) to have d leading zeros

TODO Algo binding condition

This has to do with lexicographical ordering of the indices when collisions are found and is important.
