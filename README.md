# PhTM* - An Efficient Phase-Based Transactional Memory

This repository contains code used to obtain the experimental results published on:
* IPDS 2017 - *Revisiting Phased Transactional Memory* - DOI: https://doi.org/10.1145/3079079.3079094
* TPDS 2018 - *The Case for Phase-Based Transactional Memory* - DOI: https://doi.org/10.1109/TPDS.2018.2861712

The repository is organized as follows:

- PhTM-Star/  -- *repository root.*
  - allocators/  -- *various memory allocators implementations we have tested.*
  - htm/  -- *small library to used HTM (HLE and RTM from Intel TSX. PorwerTM from IBM Power8).*
  - microbench/  -- *microbench marks used in the evaluation.*
  - msr/  -- *small library to programmer Intel PMU and collect events of code blocks.*
  - NOrec/  -- *reduced version of [RSTM library](http://www.cs.rochester.edu/research/synchronization/rstm/)
  and Hybrid TM implementations (HyNOrec, RH_NOrec and HyCO).*
  - phasedTM/  -- *base implementation of Phased TM - PhTM (PROTOTYPE) and PhTM\* (OPTIMIZED).*
  - scripts/  -- *utility scripts to compile, execute and plot results.*
  - stamp/  -- *STAMP benchmarks with dual-transactional code paths (a.k.a. FastPath and SlowPath).*
  - tinySTM-1.0.4/  -- *[TinySTM 1.0.4](http://www.tmware.org/tinystm.html) (not used in the experimental evaluation).*
  - wlpdstm-20110815/ -- *[SwissTM](http://lpd.epfl.ch/transactions/wiki/doku.php?id=swisstm) (not used in the experimental evaluation).*
