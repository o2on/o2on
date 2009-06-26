<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <xsl:apply-templates select="keys"/>
  </xsl:template>
  <xsl:template match="keys">
    <div id="body" class="body">
      <div class="category">
        <form name="d" method="POST" action="/xml/query">
          スレッド検索
          <table width="100%">
            <tr>
              <th>URL</th>
              <td><input type="text" name="url" size="100" maxlength="96"/></td>
            </tr>
            <tr>
              <th>メモ</th>
              <td><input type="text" name="note" size="50" maxlength="32"/></td>
            </tr>
          </table>
          <input type="button" class="btn" style="width:80px" value="登録" onClick="this.disabled = true;document.forms[0].submit();" />
          <input type="hidden" name="act" value="add"/>
          <input type="hidden" name="hash"/>
        </form>
      </div>
      <div class="infomessage {info/message_type}">
        <xsl:value-of select="info/message"/>
      </div>
      <table width="100%">
        <tr>
          <td class="layout" valign="bottom" width="40%">
            <span>
              <xsl:value-of select="info/count"/>スレッド　
            </span>
            <input type="button" value="全" onClick="checkAllThread(true)"/>
            <input type="button" value="解" onClick="checkAllThread(false)"/>
            <input type="button" value="無効にする" onClick="deactivateQuery('')"/>
            <input type="button" value="有効にする" onClick="activateQuery('')"/>
            <input type="button" value="削除" onClick="deleteQuery('')"/>
          </td>
          <td class="layout" width="20%">
            <input type="button" class="btn" value="再表示" onClick="Reload()"/>
          </td>
          <td class="layout" width="40%" align="right">
          </td>
        </tr>
      </table>
      <table width="100%">
        <tr>
          <th></th>
          <th class="sort" onClick="Sort('date','text')">補完日時</th>
          <th class="sort" onClick="Sort('size','number')">所有size</th>
          <th class="sort" onClick="Sort('title','text')">タイトル / メモ</th>
          <th class="sort" onClick="Sort('url','text')">URL / Hash</th>
        </tr>
        <xsl:apply-templates select="key">
          <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
        </xsl:apply-templates>
      </table>
    </div>
  </xsl:template>
  <xsl:template match="key">
  <xsl:param name="limit" select="LIMIT"/>
    <tr>
      <td class="{enable}">
        <input type="checkbox" class="checkbox btn" name="thread" value="{hash}"/>
        <xsl:if test="enable = 'enable'">
          <input type="button" value="無効にする" onClick="deactivateQuery('{hash}')"/>
        </xsl:if>
        <xsl:if test="enable = 'disable'">
          <input type="button" value="有効にする" onClick="activateQuery('{hash}')"/>
        </xsl:if>
        <input type="button" value="削除" onClick="deleteQuery('{hash}')"/>
      </td>
      <td class="{enable} C">
        <xsl:value-of select="date"/>
      </td>
      <td class="{enable} R">
        <xsl:value-of select="format-number(size, '###,###,###,###')"/>
      </td>
      <td class="{enable}" valign="top">
        <xsl:if test="size and size != 0 and title and title != ''">
          <a href="javascript:openDat('{hash}')">
            <xsl:value-of select="title"/>
          </a>
        </xsl:if>
        <xsl:if test="not(size) or size = 0 or not(title) or title = ''">
          <xsl:value-of select="title"/>
        </xsl:if>
        <br/>
        <xsl:value-of select="note"/>
      </td>
      <td class="{enable}">
        <span onClick="selectText(this)">
          <xsl:value-of select="url"/>
        </span>
        <br/>
        <xsl:value-of select="hash"/>
      </td>
    </tr>
  </xsl:template>
</xsl:stylesheet>
