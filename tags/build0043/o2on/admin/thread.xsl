<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

	<xsl:template match="/">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="threads">
	  <form name="d" method="POST">
  	  <div id="body" class="body" style="white-space:nowrap">
  	    <xsl:if test="message and message != ''">
          <xsl:value-of select="message"/>
  	    </xsl:if>
        <xsl:if test="count(thread) > 0">
        	<table width="100%">
        	  <tr>
        	    <td class="layout" width="33%">
                <input type="button" value="全選択" onClick="checkAllItem(true)"/>
                <input type="button" value="全解除" onClick="checkAllItem(false)"/>
                <input type="button" style="width:120px" value="削除" onClick="deleteThread()"/>
              </td>
              <td class="layout" width="34%">
              </td>
        	    <td class="layout" align="right" width="33%">
                <xsl:if test="dir">
                  [
                  <b><xsl:value-of select="dir"/></b>
                  ]
                  : 
                  <b><xsl:value-of select="datnum"/></b>個
                  / 
                  <b><xsl:value-of select="format-number(datsize, '###,###,###,##0')"/></b> byte
                </xsl:if>
              </td>
        	  </tr>
        	</table>
          <table>
          	<th></th>
          	<th class="sort" onClick="Sort('title','text')">タイトル</th>
          	<th class="sort" onClick="Sort('date','text')">スレ立て日時</th>
          	<th class="sort" onClick="Sort('size','number')">サイズ</th>
          	<th class="sort" onClick="Sort('url','text')">URL</th>
      			<xsl:apply-templates select="thread">
              <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
            </xsl:apply-templates>
         	</table>
        </xsl:if>
      </div>
    </form>
	</xsl:template>

	<xsl:template match="thread">
	  <tr>
	    <td>
	      <input type="checkbox" class="checkbox" name="{hash}" value=""/>
	    </td>
	    <td>
        <a href="javascript:openDat('{hash}')">
          <xsl:if test="title != ''">
    	  	  <xsl:value-of select="title"/>
          </xsl:if>
          <xsl:if test="title = ''">
            ???
          </xsl:if>
        </a>
      </td>
	    <td>
  			<xsl:value-of select="date"/>
	    </td>
	    <td class="R">
        <xsl:value-of select="format-number(size, '###,###,###,###')"/>
	    </td>
	    <td onClick="selectText(this)">
  			<xsl:value-of select="url"/>
	    </td>
	  </tr>
	</xsl:template>

</xsl:stylesheet>
