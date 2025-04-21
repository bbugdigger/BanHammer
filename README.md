# BanHammer

The rise of cheating in the gaming industry is hurting both players and developers—especially smaller studios that can’t afford expensive anti-cheat solutions. Seeing my favorite childhood games lose their reputation to cheaters inspired me to create this open-source project.  

### **WIP/Testing**  
- **Kernel-level detection**
  - Remote thread creation detection
  - Scan for blacklisted processes (just by name for now)
  - Loaded modules integrity checking (hashing .text section or ci.dll)
  - Game process handle access right stripping
  - HWID collection
  - Checking out traces in PiDDB cache table
- **External cheat detection**
  - Scanning for suspicios overlays (large window + above game + some combination of layered, transparent and topmost flags + unknown/uncommon process = flag)
  - Checkout legit processes with overlays who have potential to be abused (Discord, Dwm, Steam, Teamspeak, NVIDIA GeForce Experience, AMD Adrenalin, Razer Cortex?, Overwolf, Game Bar, OBS)
    - looking for changed overlay flags
    - looking for IAT hooks or regular hooks
- **Internal cheat detection** (DLL injection, hooks)  

This is just the beginning—I plan to expand features gradually with community support. If you're a developer, contributor, or just passionate about fair play, let’s work together to make gaming cheat-free!  

---

### **Features Planned**  

- **Kernel Driver**
  - Scan for unknown DLL modules
  - Scan for blacklisted drivers (blacklisted by signature/timestamp?)
  - Log all loaded drivers
  - Check for patches of some kernel drivers
  - Hypervisor and vm detection
  - Detection of test signing
  - Look for more driver traces: MmUnloadedDrivers, KernelHashBucketList, PoolBigPageTable, ExpCovUnloadedDrivers, Object directory?, pool tag scanning

- **Aditional Security Thoughts**
  - use lazy import library
  - rtti obfuscation
  - 

---

**Contribute:** Found a bug? Want to help? Open an [Issue](https://github.com/bbugdigger/BanHammer/issues) or submit a PR!  

#### Inspirations/Links/Knowledge/Resources Used

- https://github.com/mq1n/NoMercy
- 
