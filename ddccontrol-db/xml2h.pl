open(FILE, $ARGV[0]);
open(HEADER, ">$ARGV[0].h");

print HEADER "//Dummy header file used for translation generation...\n";
print HEADER "#define DUMMY {\n";

for (<FILE>) {
	if ($_ =~ /name=\"(.*?)\"/) {
		print HEADER "printf(_(\"$1\"));\n";
	}
}

print HEADER  "}\n";

close(HEADER);
close(FILE);
