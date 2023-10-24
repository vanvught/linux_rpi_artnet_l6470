/*
 * artnetnode_ports.h
 */

#ifndef ARTNETNODE_PORTS_H_
#define ARTNETNODE_PORTS_H_


namespace artnetnode {
#if !defined(LIGHTSET_PORTS)
# define LIGHTSET_PORTS	0
#endif

#if (LIGHTSET_PORTS == 0)
 static constexpr uint32_t MAX_PORTS = 1;	// ISO C++ forbids zero-size array
#else
 static constexpr uint32_t MAX_PORTS = LIGHTSET_PORTS;
#endif
}  // namespace artnetnode

#endif /* IARTNETNODE_PORTS_H_ */
