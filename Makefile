SRCDIR       =  prs
CXXFLAGS	 =  -O2 -g -Wall -fmessage-length=0 -I../boolean -I../common -I../parse -I../parse_expression -I../parse_ucs -I../interpret_ucs -I../ucs -I../interpret_boolean -I../parse_dot
SOURCES	    :=  $(shell find $(SRCDIR) -name '*.cpp')
OBJECTS	    :=  $(SOURCES:%.cpp=%.o)
TARGET		 =  lib$(SRCDIR).a

all: $(TARGET)

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

%.o: $(SRCDIR)/%.cpp 
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	
clean:
	rm -f $(OBJECTS) $(TARGET)
