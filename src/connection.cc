#include "connection.h"

#include <cassert>


namespace conntrackex {

using namespace std;

bool Connection::isConnTrackSupported(nf_conntrack* ct)
{
    // We only want TCP connections:
    if (nfct_get_attr_u8(ct, ATTR_L4PROTO) != IPPROTO_TCP)
        return false;

    return true;
}

Connection::Connection(nf_conntrack* ct)
{
    this->conntrack = nfct_clone(ct);
}

Connection::~Connection()
{
    nfct_destroy(this->conntrack);
    this->conntrack = NULL;
}

string Connection::getRemoteIP() const
{
    // Alternative: ATTR_ORIG_IPV4_DST
    return ip32ToString(nfct_get_attr_u32(this->conntrack, ATTR_ORIG_IPV4_SRC));
}

uint16_t Connection::getRemotePort() const
{
    // Alternative: ATTR_REPL_PORT_SRC
    return ntohs(nfct_get_attr_u16(this->conntrack, ATTR_ORIG_PORT_DST));
}

bool Connection::hasState() const
{
    return (nfct_get_attr_u8(this->conntrack, ATTR_TCP_STATE) != TCP_CONNTRACK_NONE);
}

ConnectionState Connection::getState() const
{
    if (nfct_attr_is_set(this->conntrack, ATTR_TCP_STATE) == -1)
        throw logic_error("Connection state not available.");

    auto tcp_state = nfct_get_attr_u8(this->conntrack, ATTR_TCP_STATE);

    // We don't expect to see MAX or IGNORE.
    assert(tcp_state != TCP_CONNTRACK_MAX);
    assert(tcp_state != TCP_CONNTRACK_IGNORE);

    // Calling this method on a connection with no state is a bug:
    if (!this->hasState())
        throw logic_error("Connection state not available.");

    switch (tcp_state)
    {
        case TCP_CONNTRACK_SYN_SENT:
        case TCP_CONNTRACK_SYN_SENT2:
        case TCP_CONNTRACK_SYN_RECV:
            return ConnectionState::OPENING;
        case TCP_CONNTRACK_ESTABLISHED:
            return ConnectionState::OPEN;
        case TCP_CONNTRACK_FIN_WAIT:
        case TCP_CONNTRACK_CLOSE_WAIT:
        case TCP_CONNTRACK_LAST_ACK:
        case TCP_CONNTRACK_TIME_WAIT:
            return ConnectionState::CLOSING;
        case TCP_CONNTRACK_CLOSE:
            return ConnectionState::CLOSED;
    }

    assert(false);
    return ConnectionState::CLOSED;
}

string Connection::toString() const
{
    stringstream output;
    output
        << "{"
        << "\"remote_host\":\"" << this->getRemoteIP().c_str() << ":" << this->getRemotePort() << "\","
        << "\"state\":\"" << (this->hasState() ? this->getStateString().c_str() : "None") << "\""
        << "}";
    return output.str();
}

string Connection::toNetFilterString() const
{
    char buffer[1024];
    nfct_snprintf(buffer, sizeof(buffer), this->conntrack, NFCT_T_ALL, NFCT_O_DEFAULT, NFCT_OF_TIME | NFCT_OF_TIMESTAMP | NFCT_OF_SHOW_LAYER3);
    return string(buffer);
}

bool Connection::operator<(const Connection& other) const
{
    return (nfct_get_attr_u32(this->conntrack, ATTR_ID) < nfct_get_attr_u32(other.conntrack, ATTR_ID));
}

bool Connection::operator==(const Connection& other) const
{
    return (nfct_compare(this->conntrack, other.conntrack) == 1);
}

string Connection::ip32ToString(uint32_t ip32)
{
    char output[INET_ADDRSTRLEN];

    return (inet_ntop(AF_INET, (void*)&ip32, output, INET_ADDRSTRLEN) != NULL) ?
        string((char*)&output) :
        string("");
}

string Connection::stateToString(ConnectionState state)
{
    switch (state)
    {
        case ConnectionState::OPENING: return "Opening";
        case ConnectionState::OPEN: return "Open";
        case ConnectionState::CLOSING: return "Closing";
        case ConnectionState::CLOSED: return "Closed";
    }

    assert(false);
    return "";
}

} // namespace conntrackex
