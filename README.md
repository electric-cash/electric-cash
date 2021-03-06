https://electriccash.global

What is Electric Cash?
----------------
Electric Cash is a payment protocol based on Bitcoin. It focuses on reducing transaction fees to facilitate everyday payments.   
Its comprehensive ecosystem aims to solve a major cryptocurrency industry problem concerning the blockchain fees and performance.  
|||
| --------------------- | ---------------------------------- |
| Coin name             | ELCASH                             | 
| Pre-mining | ELCASH 2,100,000  *(pre-mined 10 percent of total supply will be allocated to activities including project development, marketing, promotional efforts and more.)*                                          | 
| Total number of coins | ELCASH 21,000,000                  | 
| Initial block reward  | ELCASH 500 ([full schedule](MINING.md)) |
| Average block time    | 10 min                             |
| Algorithm             | SHA256 PoW                         |

The core of Elecric Cash protocol
---------------------------------

### Staking
Staking process to get Governance Power and earn additional benefits like free transactions and staking rewards. 

### Governance
Community-driven governance system on the network. Every user who has Governance Power can vote on the future development of the ecosystem.  

### Improved transactions
Fast and free transactions secured by the Proof of Work consensus. 

For more information, read the original [whitepaper](#).

License
-------

Elecric Cash Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/electric-cash/electric-cash/tags) are created
regularly to indicate new official, stable release versions of Elecric Cash Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Testing
-------

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.
