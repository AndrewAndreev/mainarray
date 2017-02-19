# mainarray
Proof of concept. C program runs main[] - array instead of the main function.

**main.c** is Visual Studio specific implementation of the concept for Windows.

# «Hello World!» in C with int main[] entry point.
----
_This article is a translation of my previously released article on [habrahabr.ru](https://habrahabr.ru/post/275861/)_

I would like to tell you about my implementation of «Hello, World!» in C. To heat up your attention I am going to show you the code.
```C
#include <stdio.h>
const void *ptrprintf = printf;
#pragma section(".exre", execute, read)
__declspec(allocate(".exre")) int main[] =
{
    0x646C6890, 0x20680021, 0x68726F57,
    0x2C6F6C6C, 0x48000068, 0x24448D65,
    0x15FF5002, &ptrprintf, 0xC314C483
};
```

#Introduction
So, my research started with this [article](https://jroweboy.github.io/c/asm/2015/01/26/when-is-main-not-a-function.html) which inspired me to find if there was a way to implement it on Windows.

In that article output has been implemented by _syscalls_, but in Windows it simply doesn't work this way. The only way left is to use NT kernel calls. Without going deep into details of this mechanism we can use the _printf_ function.

Feeling courageous and armed with Visual Studio I started to try. I don't know why I spent so much time changing the entry point in compilation settings, but as it turned out later Visual Studio compiler doesn't even throw a warning if main is an array instead of a function, unlike in Linux which we can see in the article I've mentioned.

The basic list of problems I had faced:

1. Array located is in data section and can not be executed
2. There is no syscall available in Windows for output and printf should be used instead

Let me explain why a function call is bad in this implementation. In a nutshell, compiler inserts the call address from the symbol table. But we have an array which is completely defined by us. So we should somehow work around this and make the compiler to put the address of _printf_ in our array.

# Solving «execution data»

The first problem I had faced, as it is expected to be, was that an array being in the data section can not be executed as code. But spending some time searching through Stackoverflow and MSDN I found a solution. Visual Studio compiler supports `pragma` directive `section` so you can define a variable in a section allowed to be executed.

After confirming this, I am convinced that it works and "function-array" main easily executes `opcode ret` and doesn't throw the «Access violation» error.
```C
#pragma section(".exre", execute, read)
__declspec(allocate(".exre")) char main[] = { 0xC3 };
```
# Some assembly code

Finally, when I could execute the array I had to write the code to execute.

I decided to store the «Hello, World» message in assembly code. Here I should mention that I am bad at assembly language and my solution may be not a proper one in some way. [Stackoverfow answer](http://stackoverflow.com/a/4025307/4109062) helped me in understanding what assembly code I need to put into an array and not to make redundant function calls.
I took Notepad++ and with the help of the function _plugins->converter->«ASCII -> HEX»_ got ASCII codes of symbols.

<pre>Hello, World!</pre>
<pre>48656C6C6F2C20576F726C6421</pre>

The next thing we should do is to divide the sequence into 4 bytes and put them on the stack in reverse order. Don't forget to reverse 4 byte subsequences in little-endian.

<details><summary>Dividing, reversing.</summary><p>
Null-terminating

   `48656C6C6F2C20576F726C642100`

Dividing into 4 byte hex numbers from the end

   `00004865 6C6C6F2C 20576F72 6C642100`

Reversing in little-endian and reordering the numbers

   `0x0021646C 0x726F5720 0x2C6F6C6C 0x65480000` 
</p></details>

---

I skipped a moment when I was trying to call _printf_ directly. So all that I was able to do is to save a pointer to the function _printf_. We are going to see why soon.

```asm
#include <stdio.h>
const void *ptrprintf = printf;
void main() {
    __asm {
        push 0x0021646C ; "ld!\0"
        push 0x726F5720 ; " Wor"
        push 0x2C6F6C6C ; "llo," 
        push 0x65480000 ; "\0\0He"
        lea  eax, [esp+2] ; eax -> "Hello, World!"
        push eax ; pushing pointer to a string on the stack 
        call ptrprintf ; calling printf through a pointer
        add  esp, 20 ; cleaning the stack up
    }
}
```
Compile and see disassembler.
```asm
00A8B001 68 6C 64 21 00       push        21646Ch  
00A8B006 68 20 57 6F 72       push        726F5720h  
00A8B00B 68 6C 6C 6F 2C       push        2C6F6C6Ch  
00A8B010 68 00 00 48 65       push        65480000h  
00A8B015 8D 44 24 02          lea         eax,[esp+2]  
00A8B019 50                   push        eax  
00A8B01A FF 15 00 90 A8 00    call        dword ptr [ptrprintf (0A89000h)]  
00A8B020 83 C4 14             add         esp,14h  
00A8B023 C3                   ret  
```
From this we can take code bytes. 

<details>
<summary>Automating this process by using regular expressions in Notepad++.</summary><p>
Regular expression for the text after the code bytes:

<pre> {2} *.*</pre>


Replace matches with nothing to delete.

To remove text before the code bytes I used Notepad++ TextFx plugin:
* Select all lines
* Go to: _TextFX->«TextFx Tools»->«Delete Line Numbers or First Word»_

Once those operations are done we almost have our complete sequence of the code bytes for the array.
```asm
68 6C 64 21 00
68 20 57 6F 72
68 6C 6C 6F 2C
68 00 00 48 65
8D 44 24 02
50
FF 15 00 90 A8 00 ; After `FF 15` bytes, the next 4 bytes should be the address of a function to call
83 C4 14
C3
```
</p></details>

---

# Call a function with «predetermined» address
I was thinking for a very long time how to leave the address of a function in complete sequence of bytes given the fact that only compiler can know its address. After asking some programmers and experimenting, I understood that it can be achieved by using the pointer operator `&` to a pointer variable to a function. That's exactly what I did.
```C
#include <stdio.h>
const void *ptrprintf = printf;
void main()
{
    void *funccall = &ptrprintf;
    __asm {
        call ptrprintf
    }
}
```
![disassembler](https://habrastorage.org/files/07e/374/7de/07e3747dec7841189b0169252c478517.png)

As you can see there is exactly that address of the called function in the pointer. Exactly what we need.

# Putting it all together

So, we have a sequence of assembly code bytes between which we should leave an expression that subsequently compiler translates into a proper address for calling the _printf_ function. The address is 4 bytes long (since we write our code for 32 bit platform), that means our array should consist of 4 byte values in such a way that after `FF 15` the address should be placed.


<details>
<summary>By simple substitutions we obtain the desired sequence.</summary><p>
We take previously obtained sequence of assembly code bytes.
Given the fact that 4 bytes after `FF 15` should be a one value we format the rest of values to it. We complete the missing bytes with the nop operator with opcode 0x90.
```asm
90 68 6C 64
21 00 68 20
57 6F 72 68
6C 6C 6F 2C
68 00 00 48
65 8D 44 24 
02 50 FF 15
00 90 A8 00 ; address for calling printf
83 C4 14 C3
```
And again make 4 byte values little-endian. Multiple selection is very useful for moving columns. In Notepad++ you can use <kbd>alt</kbd> + <kbd>shift</kbd> hotkey for multiselection.
```asm
646C6890
20680021
68726F57
2C6F6C6C
48000068
24448D65
15FF5002
00000000 ; address for calling the printf, later will be replaced with an expression
C314C483
```
</p></details>

---
Now we have a sequence of 4 byte numbers and the address for calling the _printf_. So we finally can fill up our main[] array.
```C
#include <stdio.h>
const void *ptrprintf = printf;
#pragma section(".exre", execute, read)
__declspec(allocate(".exre")) int main[] =
{
    0x646C6890, 0x20680021, 0x68726F57,
    0x2C6F6C6C, 0x48000068, 0x24448D65,
    0x15FF5002, &ptrprintf, 0xC314C483
};
```
In order to make a break point in Visual Studio debugger you should change the first element of the array to 0x646C68**CC**
Launch and see what we get.

![result](https://habrastorage.org/files/32f/2e0/8a3/32f2e08a393446bc980c4ed5d19e02e5.png)

Done!
