#pragma once

enum
{
    PACKET_RRQ,   // Read Request packet
    PACKET_WRQ,   // Write Request packet
    PACKET_DATA,  // Data packet
    PACKET_ACK,   // Acknowledgement packet
    PACKET_ERROR  // Error packet
};

/*
 * Error Codes
 *
 * Value     Meaning
 * 0         Not defined, see error message (if any).
 * 1         File not found.
 * 2         Access violation.
 * 3         Disk full or allocation exceeded.
 * 4         Illegal TFTP operation.
 * 5         Unknown transfer ID.
 * 6         File already exists.
 * 7         No such user.
 */

enum
{
    ERROR_UNDEFINED,
    ERROR_NOFILE,
    ERROR_ACCESS,
    ERROR_DISKFULL,
    ERROR_ILLEGAL,
    ERROR_UNKNOWNTID,
    ERROR_FILEEXISTS,
    ERROR_NOUSER
};
