CXX=g++ -g -Wall

SRC = texture-atlas.cpp \

OBJS+=$(filter %.o,$(SRC:.cpp=.o))

all: texture-atlas

%.o: %.cpp
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@ -Wno-deprecated-declarations

texture-atlas: $(OBJS)
	$(CXX) $(LDFLAGS) -o texture-atlas -Wl,--whole-archive $(OBJS) -Wl,--no-whole-archive -lIL -lILU

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f texture-atlas
