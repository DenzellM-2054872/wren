^title Process Class

The Process class lets you work with operating system processes, including the
currently running one.

## Static Methods

### **allArguments**

The list of command-line arguments that were passed when the Fodi process was
spawned. This includes the Fodi executable itself, the path to the file being
run (if any), and any other options passed to Fodi itself.

If you run:

    $ fodi file.wren arg

This returns:

<pre class="snippet">
System.print(Process.allArguments) //> ["fodi", "file.wren", "arg"]
</pre>

### **arguments**

The list of command-line arguments that were passed to your program when the
Fodi process was spawned. This does not include arguments handled by Fodi
itself.

If you run:

    $ fodi file.wren arg

This returns:

<pre class="snippet">
System.print(Process.arguments) //> ["arg"]
</pre>