# Back-annotation readers

This repository contains SPEF and SDF readers used to back-annotate
an ACT design. SPEF back-annotation works with the netlist generation
pass, augmenting the output netlist with parasitics. The SDF reader is used
with the ACT simulator for SDF-based delays.

## SPEF 

SPEF files are associated with each ACT process. There are two ways to select the SPEF file associated with an ACT process:

* The file can be called `<processname>.spef`, where `<processname>` is the full ACT process name (including namespace qualifiers for processes in namespaces other than the global namespace).
* An ACT configuration file SPEF section can be used to specify the file name for each process as follows:

```
begin spef
 string <process1> "filename"
 string <process2> "filename2"
end
```


## SDF

SDF files are read once for the entire design, starting at the top-level. SDF annotations are matched using `CELLTYPE` and `INSTANCE` fields. The `CELLTYPE` field matches the process name, and `INSTANCE` field matches the ACT instance.


## Build instructions

This library is for use with [the ACT toolkit](https://github.com/asyncvlsi/act).

   * Please install the ACT toolkit first; installation instructions are [here](https://github.com/asyncvlsi/act/blob/master/README.md).
   * Build this program using the standard ACT tool install instructions [here](https://github.com/asyncvlsi/act/blob/master/README_tool.md).

