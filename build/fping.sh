#!/bin/sh

	echo "Checking for fping ..."

	FPING=""

	for DIR in /bin /usr/bin /sbin /usr/sbin /usr/local/bin /usr/local/sbin /opt/bin
	do
		if test -x $DIR/fping
		then
			FPING=$DIR/fping
		fi
	done

	if test "$FPING" = ""
	then
		echo "Hobbit needs the fping utility. What command should it use to run fping ?"
		read FPING
	else
		echo "Found fping in $FPING"
	fi

	NOTOK=1
	while test $NOTOK -eq 1
	do
		echo "Checking to see if '$FPING 127.0.0.1' works ..."
		$FPING 127.0.0.1
		RC=$?
		if test $RC -eq 0; then
			echo "OK, will use '$FPING' for ping tests"
			NOTOK=0
		else
			echo ""
			echo "Failed to run fping."
			echo "If fping is not suid-root, you may want to use an suid-root wrapper"
			echo "like 'sudo' to run fping."
			echo ""
			echo "Hobbit needs the fping utility. What command should it use to run fping ?"
			read FPING
		fi
	done

