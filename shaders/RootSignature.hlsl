/* Root signature for testing */
#define main \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT),"                \
"CBV(b0, space=1),"                                             \
"DescriptorTable(SRV(t0, numDescriptors=unbounded, space=1)),"  \
"CBV(b0, space=0),"                                             \
"RootConstants(b1, num32BitConstants=1, space=0)"