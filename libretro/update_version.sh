#!/bin/sh
git svn log --limit 1 | grep "|" | cut --fields=1 --delimiter=" " > svn
