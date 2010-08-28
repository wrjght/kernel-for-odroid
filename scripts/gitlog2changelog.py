#!/usr/bin/python
# Copyright 2008 Marcus D. Hanwell <marcus@cryos.org>
# Distributed under the terms of the GNU General Public License v2 or later

import string, re, os, sys, time

# check arguments
if len(sys.argv) < 2:
    print "Usage: ./scripts/%s prefix" % os.path.basename(sys.argv[0])
    sys.exit(1)

# Execute git log with the desired command line options.
prefix = sys.argv[1]
fin = os.popen('git log --summary --no-merges --tags --decorate --simplify-by-decoration --date=short --author=samsung*', 'r');

# Create a ChangeLog file in the current directory.
fout = open('ChangeLog', 'w')

# Set up the loop variables in order to locate the blocks we want
authorFound = False
dateFound = False
messageFound = False
filesFound = False
message = ""
messageNL = False
files = ""
tagFound = False
prevAuthorLine = ""

# The main part of the loop
for line in fin:
    # The commit line marks the start of a new commit object.
    if string.find(line, 'commit') >= 0:
        # Start all over again...
        authorFound = False
        dateFound = False
        messageFound = False
        messageNL = False
        message = ""
        filesFound = False
        files = ""
	tagFound = False

	if string.find(line, 'refs/') >= 0:
   	    tagList1 = re.split('refs/', line, 1)
	    tagList2 = re.split('/', tagList1[1], 1)
	    tag = tagList2[1]
	    tag = tag[0:len(tag)-2]

        continue
    # Match the author line and extract the part we want
    elif re.match('Author:', line) >=0:
        authorList = re.split(': ', line, 1)
        author = authorList[1]
        author = author[0:len(author)-1]
        authorFound = True
    # Match the date line
    elif re.match('Date:', line) >= 0:
        dateList = re.split(':   ', line, 1)
        date = dateList[1]
        date = date[0:len(date)-1]
        dateFound = True
    # The svn-id lines are ignored
    elif re.match('    git-svn-id:', line) >= 0:
        continue
    # The sign off line is ignored too
    elif re.search('Signed-off-by', line) >= 0:
        continue
    # Extract the actual commit message for this commit
    elif authorFound & dateFound & messageFound == False:
        # Find the commit message if we can
        if len(line) == 1:
            if messageNL:
                messageFound = True
            else:
                messageNL = True
        elif len(line) == 4:
            messageFound = True
        else:
            if len(message) == 0:
                message = message + line.strip()
            else:
                message = message + " " + line.strip()
    # If this line is hit all of the files have been stored for this commit
    elif re.search('files changed', line) >= 0:
        filesFound = True
        continue
    # Collect the files for this commit. FIXME: Still need to add +/- to files
    elif authorFound & dateFound & messageFound:
        fileList = re.split(' \| ', line, 2)
        if len(fileList) > 1:
            if len(files) > 0:
                files = files + ", " + fileList[0].strip()
            else:
                files = fileList[0].strip()
    # All of the parts of the commit have been found - write out the entry
    if authorFound & dateFound & messageFound:
    	if string.find(tag, 'refs') >= 0:
	    continue
	
	if string.find(tag, prefix) < 0:
	    continue

    	# Assemble the actual commit message line(s) and limit the line length
        # to 80 characters.
        commitLine = "- " + message
        i = 0
	first = True
        commit = ""
        while i < len(commitLine):
            if len(commitLine) < i + 70:
	        if first:
                    commit = commit + "\n" + commitLine[i:len(commitLine)]
	    	    first = False
		else:
                    commit = commit + "\n\t" + commitLine[i:len(commitLine)]
                break
            index = commitLine.rfind(' ', i, i+70)
	    first = False
            if index > i:
	    	if i > 0:
                    commit = commit + "\n\t" + commitLine[i:index]
	    	else:
                    commit = commit + "\n" + commitLine[i:index]
                i = index+1
            else:
                commit = commit + "\n\t" + commitLine[i:70]
                i = i+71

        # Write out the commit line
        fout.write(commit + "\n")

        # First the author line, only outputted if it is the first for that
        # author on this day
	authorLine = re.split(' <', author, 2)
	authorName = authorLine[0]
        Title = "\t(" + tag + " by " + authorName + " at " + date + ")\n"
        fout.write(Title)

	#Now reset all the variables ready for a new commit block.
        authorFound = False
        dateFound = False
        messageFound = False
        messageNL = False
        message = ""
        filesFound = False
        files = ""
        prevAuthorLine = Title

# Close the input and output lines now that we are finished.
fin.close()
fout.close()
