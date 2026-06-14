1.Start your debugger normally:
```bash
lldb ./my_program
```
2.Use code with caution.Start the
```bash
(lldb) run
```
Use code with caution.When the program hits a function expecting input (like cin or scanf), the LLDB prompt disappears. Simply type your input directly into the terminal and press Enter.

3. To interrupt the running program and get the (lldb) prompt back, press `Ctrl + C`





## set target env variable inside lldb

nano ~/.lldbinit, add following line

```text
settings set target.env-vars MY_VAR=value LINES=40 COLUMNS=80
```
