<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
    <html>
      <head>
        <script type="text/javascript" src="/o2on.js"></script>
      </head>
      <body>
    		<xsl:apply-templates select="notification"/>
      </body>
    </html>
	</xsl:template>
	<xsl:template match="notification">
	  <xsl:value-of select="date"/>
    <xsl:if test="newim">
      <script type="text/javascript">
        notifyIM();
      </script>
	  </xsl:if>
    <xsl:if test="newver">
      <script type="text/javascript">
        notifyNewVer();
      </script>
	  </xsl:if>
	</xsl:template>
</xsl:stylesheet>
