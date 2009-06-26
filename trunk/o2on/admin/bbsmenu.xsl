<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

	<xsl:template match="/">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="bbsmenu">
    <html>
      <head>
        <meta http-equiv="MSThemeCompatible" content="Yes"/>
      	<link rel="stylesheet" type="text/css" href="/style.css"/>
        <script type="text/javascript" src="/o2on.js"></script>
      </head>
      <body style="font-size:75%">
    	  <form method="POST" action="/xml/bbsmenu">
      	  <input type="button" style="width:100%" value="再表示" onClick="location.replace(location.href)"/><br/>
      	  <input type="button" style="width:100%" value="確定" onClick="this.disabled = true;document.forms[0].submit();"/><br/>

      	  <input type="button" class="btn0" value="板取得" onClick="getBoards()"/>
      	  <input type="button" class="btn0" value="全" onClick="checkAllBoard(true)"/>
      	  <input type="button" class="btn0" value="解" onClick="checkAllBoard(false)"/>
      	  <br/><br/>
          <input type="text" name="bbsurl" style="width:95%"/><br/>
          <input type="button" class="btn0" value="板追加" onClick="submit()"/>
      	  <br/>
          <xsl:apply-templates select="category"/>
      	</form>
      </body>
    </html>
	</xsl:template>

	<xsl:template match="category">
    <br/>
    <b style="color:#cc3300">
      <input type="checkbox" class="checkbox" onClick="checkCategory(this,'{categoryname}')"/>
      <xsl:value-of select="categoryname"/>
    </b>
    <div id="{categoryname}">
   	  <xsl:apply-templates select="board"/>
    </div>
	</xsl:template>

	<xsl:template match="board">
    <xsl:if test="datnum = '0'">
      <div class="grayed">
        <xsl:if test="enable = '0'">
          <input type="checkbox" class="checkbox" name="{domain}:{bbsname}"/>
        </xsl:if>
        <xsl:if test="enable != '0'">
          <input type="checkbox" class="checkbox" name="{domain}:{bbsname}" checked="1"/>
        </xsl:if>
  			<xsl:value-of select="title"/>
  		</div>
  	</xsl:if>
    <xsl:if test="datnum != '0'">
      <div style="background-color:rgb({round(239*(100-ratio) div 100)},239,239)">
        <xsl:if test="enable = '0'">
          <input type="checkbox" class="checkbox" name="{domain}:{bbsname}"/>
        </xsl:if>
        <xsl:if test="enable != '0'">
          <input type="checkbox" class="checkbox" name="{domain}:{bbsname}" checked="1"/>
        </xsl:if>
        <a href="javascript:openThreadList('{domain}','{bbsname}')">
    			<xsl:value-of select="title"/>
        </a>
  		</div>
  	</xsl:if>
	</xsl:template>

</xsl:stylesheet>
