<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
    <html>
      <head>
        <meta http-equiv="Refresh" content="30"/>
        <link rel="stylesheet" type="text/css" href="../style.css"/>
      </head>
      <body class="broadcast">
    		<xsl:apply-templates select="messages"/>
      </body>
    </html>
	</xsl:template>
	<xsl:template match="messages">
    <xsl:apply-templates select="message"/>
	</xsl:template>
	<xsl:template match="message">
	  <xsl:if test="mine = '1'">
      <div class="mine">
    		<xsl:value-of select="date"/>
        &#160;
       	<xsl:value-of select="name"/>
        &gt;&#160;
        <xsl:value-of select="msg"/>
      </div>
    </xsl:if>
	  <xsl:if test="mine = '0'">
      <div>
    		<xsl:value-of select="date"/>
        &#160;
        <xsl:if test="port = '0'">
        	<xsl:if test="not(name) or name = ''">
        	  ?
        	</xsl:if>
        	<xsl:if test="name and name != ''">
        		<xsl:value-of select="name"/>
        	</xsl:if>
        </xsl:if>
        <xsl:if test="port != '0'">
          <a href="javascript:void(0)" onClick="top.main.showMsgForm2(50,120,'{id}','{ip}','{port}','{name}','{pubkey}')">
        	  <xsl:if test="not(name) or name = ''">
        	    ?
        	  </xsl:if>
        	  <xsl:if test="name and name != ''">
        			<xsl:value-of select="name"/>
        	  </xsl:if>
          </a>
        </xsl:if>
        &gt;&#160;
        <xsl:value-of select="msg"/>
      </div>
    </xsl:if>
	</xsl:template>
</xsl:stylesheet>
