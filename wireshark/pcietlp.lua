--
-- Based on https://github.com/NetTLP/wireshark-nettlp.
--

local tlp_proto = Proto("PCIeTLP", "PCI Express Transaction Layer Packet")

local tlp_f = tlp_proto.fields

-- PCI Express TLP Common Header
-- |       0       |       1       |       2       |       3       |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- | FMT |   Type  |R| TC  |   R   |T|E|Atr| R |       Length      |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

local TLPPacketFmtType = {
	[0x00] =  "MRd",
	[0x20] =  "MRd",
	[0x01] =  "MRdLk",
	[0x21] =  "MRdLk",
	[0x40] =  "MWr",
	[0x60] =  "MWr",
	[0x02] =  "IORd",
	[0x42] =  "IOWr",
	[0x04] =  "CfgRd0",
	[0x44] =  "CfgWr0",
	[0x05] =  "CfgRd1",
	[0x45] =  "CfgWr1",
	[0x1b] =  "TCfgRd",
	[0x5b] =  "TCfgWr",
	[0x30] =  "Msg",
	[0x70] =  "MsgD",
	[0x0a] =  "Cpl",
	[0x4a] =  "CplD",
	[0x0b] =  "CplLk",
	[0x4b] =  "CplDLk",
	[0x4c] =  "FetchAdd",
	[0x6c] =  "FetchAdd",
	[0x4d] =  "Swap",
	[0x6d] =  "Swap",
	[0x4e] =  "CAS",
	[0x6e] =  "CAS",
	[0x80] =  "LPrfx",
	[0x90] =  "EPrfx"
}
tlp_f.tlp_fmttype = ProtoField.uint8("tlp.fmttype", "Packet Format Type", base.HEX, TLPPacketFmtType)

local TLPPacketFormat = {
	[0x0] = "3DW",
	[0x1] = "4DW",
	[0x2] = "3DW",
	[0x3] = "4DW",
	[0x4] = "TLP_Prefix"
}
tlp_f.tlp_fmt = ProtoField.uint8("tlp.fmttype.format", "Packet Format", base.HEX, TLPPacketFormat, 0xe)

local TLPPacketType = {
	[0x00] = "MRd or MWr",
	[0x01] = "MRdLk",
	[0x02] = "IORd or IOWr",
	[0x04] = "CfgRd0 or CfgWr0",
	[0x05] = "CfgRd1 or CfgWr1",
	[0x1b] = "TCfgRd or TCfgWR",
	[0x10] = "Msg or MsgD",
	[0x0a] = "Cpl or CplD",
	[0x0b] = "CplLk or CplDLk",
	[0x0c] = "FetchAdd",
	[0x0d] = "Swap",
	[0x0e] = "CAS"
}
tlp_f.tlp_pkttype = ProtoField.uint8("tlp.fmttype.pkttype", "Packet Type", base.HEX, TLPPacketType, 0x1f)

tlp_f.tlp_rsvd0 = ProtoField.uint8("tlp.reserved1", "Reserved0", nil, nil, 0x80)
tlp_f.tlp_tclass = ProtoField.uint8("tlp.tclass", "Tclass", base.HEX, nil, 0x70)
tlp_f.tlp_rsvd1 = ProtoField.uint8("tlp.reserved2", "Reserved1", nil, nil, 0xf)

tlp_f.tlp_digest = ProtoField.uint16("tlp.digest", "Digest", base.HEX, nil, 0x8000)
tlp_f.tlp_poison = ProtoField.uint16("tlp.poison", "Poison", base.HEX, nil, 0x4000)
local TLPAttr = {
	[0x0] = "",
	[0x1] = "NOSNP",
	[0x2] = "RELAX",
	[0x3] = "RELAX, NOSNP",
}
tlp_f.tlp_attr = ProtoField.uint16("tlp.attr", "Attr", base.HEX, TLPAttr, 0x3000)
tlp_f.tlp_rsvd2 = ProtoField.uint16("tlp.reserved3", "Reserved2", nil, nil, 0xc00)
tlp_f.tlp_length = ProtoField.uint16("tlp.length", "Length", base.HEX, nil, 0x3ff)

tlp_f.tlp_payload = ProtoField.bytes("tlp.payload", "TLP Payload")

tlp_f.tlp_reqid = ProtoField.uint16("tlp.reqid", "Requester ID", base.HEX)
tlp_f.tlp_cplid = ProtoField.uint16("tlp.cplid", "Completer ID", base.HEX)

tlp_f.tlp_busnum = ProtoField.uint16("tlp.busnum", "Bus Number", base.HEX)
tlp_f.tlp_devnum = ProtoField.uint16("tlp.devnum", "Device Number", base.HEX)
tlp_f.tlp_funcnum = ProtoField.uint16("tlp.funcnum", "Function Number", base.HEX)


-- PCI Express TLP Memory Request 3DW Header (32 bit address):
-- |       0       |       1       |       2       |       3       |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- | FMT |   Type  |R| TC  |   R   |T|E|Atr| R |       Length      |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |           Request ID          |      Tag      |LastBE |FirstBE|
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |                           Address                         | R |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
--
-- or, TLP 4DW header (64 bit address):
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |                            Address                            |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |                            Address                        | R |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

tlp_f.mr_tag = ProtoField.uint8("tlp.mr_tag", "Tag", base.HEX)

tlp_f.mr_lastbe = ProtoField.uint8("tlp.mr_lastbe", "LastBE", base.HEX, nil, 0xf0)
tlp_f.mr_firstbe = ProtoField.uint8("tlp.mr_firstbe", "FirstBE", base.HEX, nil, 0xf)

tlp_f.mr_addr32 = ProtoField.uint32("tlp.mr_addr32", "Address 32 bit", base.HEX)
tlp_f.mr_addr32_rsvd = ProtoField.uint32("tlp.mr_addr32_rsvd", "Reserved_32b")

tlp_f.mr_addr64 = ProtoField.uint64("tlp.mr_addr64", "Address 64 bit", base.HEX)
tlp_f.mr_addr64_rsvd = ProtoField.uint64("tlp.mr_addr64_rsvd", "Reserved_64b")

-- 
-- PCI Express TLP Completion Header
-- |       0       |       1       |       2       |       3       |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |R|Fmt|  Type   |R| TC  |   R   |T|E|Atr| R |      Length       |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |          Completer ID         |CplSt|B|      Byte Count       |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
-- |          Requester ID         |      Tag      |R| Lower Addr  |
-- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

tlp_f.cpl_cplid = ProtoField.uint16("tlp.cpl_cplid", "Completer ID", base.HEX)

local TLPCompletionStatus = {
	[0x0] = "Successful Completion (SC)",
	[0x1] = "Unsupported Request (UR)",
	[0x2] = "Configuration Reqeust Retry Status (CRS)",
	[0x3] = "Completer Abort (CA)"
}
tlp_f.cpl_cplstat = ProtoField.uint16("tlp.cpl_cplstat", "Completion Status", base.HEX, TLPCompletionStatus, 0xe000)
tlp_f.cpl_bcm = ProtoField.uint16("tlp.cpl_bcm", "Byte Count Modified", nil, nil, 0x1000)
tlp_f.cpl_bytecnt = ProtoField.uint16("tlp.cpl_bytecnt", "Byte Count", nil, nil, 0xfff)

tlp_f.cpl_reqid = ProtoField.uint16("tlp.cpl_reqid", "Requester ID", base.HEX)

tlp_f.cpl_tag = ProtoField.uint8("tlp.cpl_tag", "Tag", base.HEX)

tlp_f.cpl_rsvd0 = ProtoField.uint8("tlp.cpl_revd0", "Reserved0", base.HEX, nil, 0x80)
tlp_f.cpl_lowaddr = ProtoField.uint8("tlp.cpl_lowaddr", "Lower Address", base.HEX, nil, 0x7f)

function tlp_proto.dissector(buffer, pinfo, tree)
	if buffer:len() == 0 then return end

	-- TLP common header
	local tlp_subtree = tree:add(buffer(0, buffer:len()), "PCIe Transaction Layer Packet")
	local fmttype = buffer(0,1):uint()

	tlp_subtree:add(tlp_f.tlp_fmttype, buffer(0,1))

	tlp_subtree:add(tlp_f.tlp_rsvd0, buffer(1,1))
	tlp_subtree:add(tlp_f.tlp_tclass, buffer(1,1))
	tlp_subtree:add(tlp_f.tlp_rsvd1, buffer(1,1))

	tlp_subtree:add(tlp_f.tlp_digest, buffer(2,2))
	tlp_subtree:add(tlp_f.tlp_poison, buffer(2,2))
	tlp_subtree:add(tlp_f.tlp_attr, buffer(2,2))
	tlp_subtree:add(tlp_f.tlp_rsvd2, buffer(2,2))
	tlp_subtree:add(tlp_f.tlp_length, buffer(2,2))  -- 0-3
	--- End: TLP common header

	-- TLP memory request packet
	if fmttype == 0x00 then  -- MRd_3DW
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(4,2))
			-- reqid tree
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0, 8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8, 4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.mr_tag, buffer(6,1))
		tlp_subtree:add(tlp_f.mr_lastbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_firstbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_addr32, buffer(8,4))
		tlp_subtree:add(tlp_f.mr_addr32_rsvd, buffer(11,1):bitfield(6,2))
	elseif fmttype == 0x20 then  -- MRd_4DW
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(4,2))
			-- reqid tree
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(10,2):bitfield(0, 8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(10,2):bitfield(8, 4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(10,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.mr_tag, buffer(6,1))
		tlp_subtree:add(tlp_f.mr_lastbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_firstbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_addr64, buffer(8,8))
		tlp_subtree:add(tlp_f.mr_addr64_rsvd, buffer(15,1):bitfield(6,2))
	elseif fmttype == 0x40 then  -- MWr_3DW
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(4,2))
			-- reqid tree
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0, 8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8, 4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.mr_tag, buffer(6,1))
		tlp_subtree:add(tlp_f.mr_lastbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_firstbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_addr32, buffer(8,4))
		tlp_subtree:add(tlp_f.mr_addr32_rsvd, buffer(11,1):bitfield(6,2))
		tlp_subtree:add(tlp_f.tlp_payload, buffer(12))
	elseif fmttype == 0x60 then  -- MWr_4DW
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(4,2))
			-- reqid tree
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0, 8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8, 4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.mr_tag, buffer(6,1))
		tlp_subtree:add(tlp_f.mr_lastbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_firstbe, buffer(7,1))
		tlp_subtree:add(tlp_f.mr_addr64, buffer(8,8))
		tlp_subtree:add(tlp_f.mr_addr64_rsvd, buffer(15,1):bitfield(6,2))
		tlp_subtree:add(tlp_f.tlp_payload, buffer(16))
	-- TLP completion packet
	elseif fmttype == 0x0a then  -- Cpl
		local cplid_subtree = tlp_subtree:add(tlp_f.tlp_cplid, buffer(4,2))
			-- cplid tree
			cplid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0, 8))
			cplid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8, 4))
			cplid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.cpl_cplstat, buffer(6,2))
		tlp_subtree:add(tlp_f.cpl_bcm, buffer(6,2))
		tlp_subtree:add(tlp_f.cpl_bytecnt, buffer(6,2))
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(8,2))
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(6,2), buffer(6,2):bitfield(0, 8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(6,2), buffer(6,2):bitfield(8, 4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(6,2), buffer(6,2):bitfield(12, 4))
		tlp_subtree:add(tlp_f.cpl_tag, buffer(10,1))
		tlp_subtree:add(tlp_f.cpl_rsvd0, buffer(11,1))
		tlp_subtree:add(tlp_f.cpl_lowaddr, buffer(11,1))
	elseif fmttype == 0x4a then  -- CplD
		local cplid_subtree = tlp_subtree:add(tlp_f.tlp_cplid, buffer(4,2))
			-- cplid tree
			cplid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0,8))
			cplid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8,4))
			cplid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12,4))
		tlp_subtree:add(tlp_f.cpl_cplstat, buffer(6,2))
		tlp_subtree:add(tlp_f.cpl_bcm, buffer(6,2))
		tlp_subtree:add(tlp_f.cpl_bytecnt, buffer(6,2))
		local reqid_subtree = tlp_subtree:add(tlp_f.tlp_reqid, buffer(8,2))
			-- reqid tree
			reqid_subtree:add(tlp_f.tlp_busnum, buffer(4,2), buffer(4,2):bitfield(0,8))
			reqid_subtree:add(tlp_f.tlp_devnum, buffer(4,2), buffer(4,2):bitfield(8,4))
			reqid_subtree:add(tlp_f.tlp_funcnum, buffer(4,2), buffer(4,2):bitfield(12,4))
		tlp_subtree:add(tlp_f.cpl_tag, buffer(10,1))
		tlp_subtree:add(tlp_f.cpl_rsvd0, buffer(11,1))
		tlp_subtree:add(tlp_f.cpl_lowaddr, buffer(11,1))
		tlp_subtree:add(tlp_f.tlp_payload, buffer(12))
	end
	-- End: TLP memory request packet

	-- pinfo
	pinfo.cols.protocol = "PCIe TLP"

	local fmttype_info = TLPPacketFmtType[fmttype]
	local fmt_info = TLPPacketFormat[buffer(0,1):bitfield(3,5)]
	local tclass_info = buffer(1,1):bitfield(1,3)
	local attr_info = TLPAttr[buffer(2,1):bitfield(2,2)]
	local tlplen_info = buffer(2,2):bitfield(6,10)

	local pinfo_str = string.format("%s, %s, tc %d, flags [%s], attrs [%s], len %d DW, ",
		fmttype_info, fmt_info, tclass_info, "", attr_info, tlplen_info)

	if fmttype == 0x00 or fmttype == 0x40 then  -- MRd_3DW or MWr_3DW
		local reqid_busnum_info = buffer(4,1):uint()
		local reqid_devnum_info = buffer(5,1):bitfield(0,4)
		local tag_info = buffer(6,1):uint()
		local last_info = buffer(7,1):bitfield(0,4)
		local first_info = buffer(7,1):bitfield(4,4)
		local addr32_info = buffer(8,4):uint()
		pinfo_str = pinfo_str .. string.format("requester %02x:%02x, tag 0x%02x, last 0x%x, first 0x%x, Addr 0x%016x",
			reqid_busnum_info, reqid_devnum_info, tag_info, last_info, first_info, addr32_info)
	elseif fmttype == 0x20 or fmtype == 0x60 then  -- MRd_4DW or MWr_4DW
		local reqid_busnum_info = buffer(4,1):uint()
		local reqid_devnum_info = buffer(5,1):bitfield(0,4)
		local tag_info = buffer(6,1):uint()
		local last_info = buffer(7,1):bitfield(0,4)
		local first_info = buffer(7,1):bitfield(4,4)
		local addr64_info = buffer(8,8):uint()
		pinfo_str = pinfo_str .. string.format("requester %02x:%02x, tag 0x%02x, last 0x%x, first 0x%x, Addr 0x%032x",
			reqid_busnum_info, reqid_devnum_info, tag_info, last_info, first_info, addr64_info)
	elseif fmttype == 0x0a or fmttype == 0x4a then  -- Cpl or CplD
		local cplid_busnum_info = buffer(4,1):uint()
		local cplid_devnum_info = buffer(5,1):bitfield(0,4)
		local stat_info = TLPCompletionStatus[buffer(6,2):bitfield(0,3)]
		local bytecnt_info = buffer(6,2):bitfield(4,12)
		local reqid_busnum_info = buffer(8,1):uint()
		local reqid_devnum_info = buffer(9,1):bitfield(0,4)
		local tag_info = buffer(10,1):uint()
		local lowaddr_info = buffer(11,1):bitfield(1,7)
		pinfo_str = pinfo_str .. string.format("completer %02x:%02x, %s, byte count %d, requester %02x:%02x, tag 0x%02x, lowaddr 0x%02x",
			cplid_busnum_info, cplid_devnum_info, stat_info, bytecnt_info, 
			reqid_busnum_info, reqid_devnum_info, tag_info, lowaddr_info)
	end

	pinfo.cols.info = pinfo_str
	-- End: pinfo
end

DissectorTable.get("udp.port"):add("9999", tlp_proto)
