WavAlign
===================

WavAlign is a command line utility primarly designed to align reference and decompressed wav files.

## Dependencies
There is no external dependencies to download, but project itself uses the [kissfft](https://github.com/mborgerding/kissfft) library in a form of source code.

## Compile
Get the code:

	git clone https://github.com/DmitryYudin/wavalign
	cd wavalign

Then just use:

	cmake -B out -S .
	cmake --build out --config Release

In a result of above, the 'wavalign' executable should appear in the 'out/Release' folder.

## Use
The empty command line or '--help' option will display the help message. The following example covers most use cases:

	wavalign ref.wav tst.wav aligned.wav

where

	ref.wav     - input reference file
	tst.wav     - input decompressed file
	aligned.wav - output file
