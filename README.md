# POC CVE-2022-21877

This repository contains a POC for the CVE-2022-21877, found by [Quang Linh](https://twitter.com/linhlhq), working at [STAR Labs](https://twitter.com/starlabs_sg). This is an information leak found inside the spaceport.sys driver.

An accompying blogpost can be found at this [address](https://big5-sec.github.io/posts/an-analysis-of-cve-2022-21877/).

## Using it

To build the POC, simply run ``.\build``.

To run it, you need a pool on your machine that can get a Tier. In my tests, this means having at least two storage pools, the primordial one and one another. All of this because the primordial pool cannot have a Tier attached.

To get the necessary configuration, you can set up 3 virtuals disks on a virtual machine. These will be used automatically as the primordial pool by Microsoft. You can then reboot, add another two virtual disks. You can then create the second pool with the following command, as an administrator:

```PS1
New-StoragePool -FriendlyName Pool2 -StorageSubsystemFriendlyName "Windows Storage*" -PhysicalDisks (Get-PhysicalDisk -CanPool $True)
```

Once all set, launch the POC as an administrator by providing it the "FriendlyName" of the usable pool (for example the one you just created).

## Results of this POC

This poc permits to leak uncontrolled data located past the system buffer. As explained in the blogpost, I was not able to trigger a controlled leak of a given kernel object. Here is what it looks like :

![](https://raw.githubusercontent.com/Big5-sec/cve-2022-21877/main/img/result.PNG)

